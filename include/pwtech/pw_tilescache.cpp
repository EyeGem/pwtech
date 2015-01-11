// pw_tilescache.cpp
#include "pw_tilescache.h"

#include "pw_lookups.h"

#include <memory.h>
#include <string.h>

#include "types/pw_array.h"
#include "types/pw_cachemap.h"

//----------------------------------------------------------------

enum
{
    ST_READY,
    ST_LOADING,
    ST_FAILED,
};

//----------------------------------------------------------------

struct SPWTilesCacheData
{
    struct SPage
    {
        PWUInt32 key;
        PWUInt32 state;
        PWUInt8 *data;
        SPage   *prevLRU;
        SPage   *nextLRU;
    };

    static const int MaxPages = 256*1024;   // 256K * 4K = 1024M = 1G (max.pages data), 256K * 64 = 16M (items), 256K * 2 * 4 = 2M (buckets)
    PWCacheMap<SPage, MaxPages> m_pagesMap; // PWUInt32 key = (((PWUInt32)tile) << 16) + (PWUInt32)page

    static const int PageSize     = 4096; // 4K
    static const int PageShift    = 12;   // 2^12 = 4K
    static const int PagesInBlock = 1024; // 1024 * 4K = 4M
    PWArray<void *> m_pagesAllocBlocks;
    PWUInt8        *m_pagesAllocPtr;
    PWUInt32        m_pagesAllocSize;

    SPage *m_headLRU;
    SPage *m_lastLRU;

    static const int MaxRequests = 256;
    PWArray<SPWTilesCacheRequest> m_requests;
    PWUInt32                      m_requestsCount;

    int      m_last_tile;
    int      m_last_level;
    PWUInt32 m_last_offset;
    PWUInt32 m_last_sum;

    SPWTilesCacheSubdiv  m_retSubdiv;
    SPWTilesCacheRequest m_retRequest;

    PWLUT_ByteCounts m_lut_byte_counts;
    PWLUT_WordCounts m_lut_word_counts;

    PWUInt8 m_tmpbuf[8192];

public:
    SPWTilesCacheData()
    {
        m_pagesAllocPtr = 0;
        m_pagesAllocSize = 0;

        m_headLRU = 0;
        m_lastLRU = 0;

        m_requestsCount = 0;

        m_last_tile   = 0;
        m_last_level  = 0;
        m_last_offset = 0;
        m_last_sum    = 0;
    }

    ~SPWTilesCacheData()
    {
        for ( int i = 0; i < m_pagesAllocBlocks.GetCount(); ++i )
            ::free( m_pagesAllocBlocks[i] );
        m_pagesAllocBlocks.Clear();
    }

public:
    void OnData( int tile, PWUInt32 offset, PWUInt32 size, const void *data )
    {
        if ( (offset & (PageSize-1)) != 0 )
            return;
        if ( size != PageSize )
            return;

        const PWUInt32 page = offset >> PageShift;
        const PWUInt32 key = (((PWUInt32)tile) << 16) + page;

        SPage *pPage = m_pagesMap.Modify( key );
        if ( !pPage || pPage->state != ST_LOADING )
            return;

        --m_requestsCount;

        if ( !data )
        {
            pPage->state = ST_FAILED;
            return;
        }

        memcpy( pPage->data, data, PageSize );
        pPage->state = ST_READY;

        if ( m_headLRU ) m_headLRU->prevLRU = pPage; else m_lastLRU = pPage;
        pPage->prevLRU = 0;
        pPage->nextLRU = m_headLRU;
        m_headLRU = pPage;
    }

public:
    int ReadTilePage( const PWUInt8 *& ptr, int tile, PWUInt32 page )
    {
        const PWUInt32 key = (((PWUInt32)tile) << 16) + page;

        SPage *pPage = m_pagesMap.Modify( key );
        if ( !pPage )
        {
            if ( m_requestsCount >= MaxRequests )
                return ST_LOADING;

            SPage *pNewPage = m_pagesMap.Create( key );
            if ( !pNewPage )
            {
                if ( !m_lastLRU )
                    return ST_FAILED;

                const PWUInt32 lastKey = m_lastLRU->key;
                PWUInt8 *lastData = m_lastLRU->data;
                
                m_lastLRU = m_lastLRU->prevLRU;
                if ( m_lastLRU ) m_lastLRU->nextLRU = 0; else m_headLRU = 0;

                m_pagesMap.Remove( lastKey );

                SPage *pNewPage = m_pagesMap.Create( key );
                //if ( !pNewPage )
                //    return ST_FAILED; // should not happen

                pNewPage->data = lastData;
            }
            else
            {
                if ( m_pagesAllocSize == 0 )
                {
                    m_pagesAllocSize = PagesInBlock * PageSize;
                    m_pagesAllocPtr = (PWUInt8 *)malloc( m_pagesAllocSize );
                    m_pagesAllocBlocks.PushBack( m_pagesAllocPtr );
                }

                pNewPage->data = m_pagesAllocPtr;
                m_pagesAllocPtr += PageSize;
                m_pagesAllocSize -= PageSize;
            }

            pNewPage->key     = key;
            pNewPage->state   = ST_LOADING;
            pNewPage->prevLRU = 0; // not in LRU list while loading
            pNewPage->nextLRU = 0; // not in LRU list while loading

            SPWTilesCacheRequest & request = m_requests.PushBack();
            //
            request.tile   = tile;
            request.offset = page << PageShift;
            request.size   = PageSize;

            ++m_requestsCount;

            return ST_LOADING;
        }

        if ( pPage->state )
        {
            ptr = 0;
            return pPage->state;
        }

        if ( pPage->prevLRU )
        {
            pPage->prevLRU->nextLRU = pPage->nextLRU;
            if ( pPage->nextLRU ) pPage->nextLRU->prevLRU = pPage->prevLRU; else m_lastLRU = pPage->prevLRU;

            if ( m_headLRU ) m_headLRU->prevLRU = pPage; else m_lastLRU = pPage;
            pPage->prevLRU = 0;
            pPage->nextLRU = m_headLRU;
            m_headLRU = pPage;
        }

        ptr = pPage->data;
        return ST_READY;
    }

    int ReadTileMem( void *dest, int tile, PWUInt32 offset, PWUInt32 size )
    {
        PWUInt32 page = offset >> PageShift;
        offset &= (PageSize-1);

        int st;
        const PWUInt8 *ptr;

        PWUInt8 *wr = (PWUInt8 *)dest;
        while ( size > 0 )
        {
            st = ReadTilePage( ptr, tile, page );
            if ( st ) return st;

            PWUInt32 sz = PageSize - offset;
            if ( sz > size )
                sz = size;

            memcpy( wr, ptr + offset, sz );

            wr += sz;
            size -= sz;

            ++page;
            offset = 0;
        }

        return ST_READY;
    }
};

//----------------------------------------------------------------

PWTilesCache::PWTilesCache() { m_ptr = new SPWTilesCacheData(); }
PWTilesCache::~PWTilesCache() { delete (SPWTilesCacheData *)m_ptr; }

//----------------------------------------------------------------

const SPWTilesCacheSubdiv *PWTilesCache::Subdiv( bool & wait, int tile, int level, PWUInt32 offset )
{
    // NO ARGS CHECKS!
    // OPTIMIZED FOR MINIMUM INSTRUCTIONS (avoid conditionals)

    SPWTilesCacheData & m = *((SPWTilesCacheData *)m_ptr);

    int st;
    const PWUInt8 *ptr;
 
    st = m.ReadTilePage( ptr, tile, 0 );
    if ( st ) { wait = (st == ST_LOADING); return 0; }

    PWUInt8 num_levels;
    memcpy( &num_levels, ptr + 10, 1 );
    if ( level >= num_levels-1 )
    {
        wait = false;
        if ( level == num_levels-1 )
        {
            SPWTilesCacheSubdiv & result = m.m_retSubdiv;
            //
            result.oct    = 0;
            result.datas  = 0;
            result.offset = 0;
            //
            return &result;
        }
        return 0;
    }

    PWUInt8 sum_shift;
    memcpy( &sum_shift, ptr + 11, 1 );

    const PWUInt32 off1 = 16 + level * 16; // max 16+29*16 = 480 (i.e. under 4K)

    PWUInt32 ofs = 0;
    PWUInt32 sum = 0;

    if ( m.m_last_tile == tile && m.m_last_level == level && m.m_last_offset < offset && (offset - m.m_last_offset) < 1024 )
    {
        ofs = m.m_last_offset;
        sum = m.m_last_sum;
    }
    else
    {
        PWUInt32 off_sums;
        memcpy( &off_sums, ptr + off1, 4 );
        
        const int chunk_idx = offset >> sum_shift;
        const PWUInt32 off2 = off_sums + 4 + chunk_idx * 4;

        if ( off2 <= 4092 )
        {
            memcpy( &sum, ptr + off2, 4 );
        }
        else
        {
            st = m.ReadTileMem( &sum, tile, off2, 4 );
            if ( st ) { wait = (st == ST_LOADING); return 0; }
        }

        ofs = chunk_idx << sum_shift;
    }

    PWUInt32 off_octs;
    memcpy( &off_octs, ptr + off1+4, 4 );

    const PWUInt32 sta3 = ofs;
    const PWUInt32 off3 = off_octs + 4 + sta3;
    const PWUInt32 len3 = offset - sta3;

    PWUInt8 *buf = m.m_tmpbuf;
    st = m.ReadTileMem( buf, tile, off3, len3+1 );
    if ( st ) { wait = (st == ST_LOADING); return 0; }

    if ( len3 > 0 )
    {
        const PWUInt16 *wptr = (const PWUInt16 *)buf;
        const int len3w = len3 >> 1;
        
        for ( int i = 0; i < len3w; ++i )
            sum += m.m_lut_word_counts.lookup[ wptr[i] ];

        if ( len3 & 1 )
            sum += m.m_lut_byte_counts.lookup[ buf[ len3-1 ] ];
    }

    m.m_last_tile = tile;
    m.m_last_level = level;
    m.m_last_offset = offset;
    m.m_last_sum = sum;

    SPWTilesCacheSubdiv & result = m.m_retSubdiv;
    //
    result.oct    = buf[ len3 ];
    result.offset = sum;

    PWUInt32 off_data;
    memcpy( &off_data, ptr + (off1+16) + 8, 4 );

    const int num_children = m.m_lut_byte_counts.lookup[ result.oct ];

    PWUInt8 itemDataSize;
    memcpy( &itemDataSize, ptr + 9, 1 );

    st = m.ReadTileMem( buf, tile, off_data + 4 + result.offset * itemDataSize, num_children * itemDataSize );
    if ( st ) { wait = (st == ST_LOADING); return 0; }

    result.datas = buf;

    wait = false;
    return &result;
}

//----------------------------------------------------------------

// 8 PWTILE01
// 1 0
// 1 itemdatasize
// 1 num_levels
// 1 sum_shift
// 4 reserved(0)
// 
// 4 offset of sums for level 0   ---> DWORD N, DWORD sum0 ... DWORD sumN-1
// 4 offset of nodes for level 0  ---> DWORD LEN, ...
// 4 offset of datas for level 0  ---> DWORD LEN, ...
// 4 reserved (level 0)
// ...

//----------------------------------------------------------------

const SPWTilesCacheRequest *PWTilesCache::GetRequests( int & count )
{
    SPWTilesCacheData & m = *((SPWTilesCacheData *)m_ptr);

    if ( m.m_requests.IsEmpty() )
    {
        count = 0;
        return 0;
    }

    count = m.m_requests.GetCount();
    return &m.m_requests[0];
}

//----------------------------------------------------------------

void PWTilesCache::GotRequests()
{
    SPWTilesCacheData & m = *((SPWTilesCacheData *)m_ptr);

    m.m_requests.Clear();
}

//----------------------------------------------------------------

void PWTilesCache::OnLoaded( const SPWTilesCacheRequest & request, const void *data )
{
    SPWTilesCacheData & m = *((SPWTilesCacheData *)m_ptr);

    m.OnData( request.tile, request.offset, request.size, data );
}

//----------------------------------------------------------------

void PWTilesCache::OnFailed( const SPWTilesCacheRequest & request )
{
    SPWTilesCacheData & m = *((SPWTilesCacheData *)m_ptr);

    m.OnData( request.tile, request.offset, request.size, 0 );
}

//----------------------------------------------------------------
