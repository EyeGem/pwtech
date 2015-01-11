// pw_rendernodes1.h
#ifndef pw_rendernodes1H
#define pw_rendernodes1H

#include "../../pw_lookups.h"

#include "../../types/pw_ints.h"
#include "../../types/pw_array.h"

#include <memory.h>
#include <string.h>

//----------------------------------------------------------------

template<int ItemDataSize>
struct SPWRenderNode1
{
    PWUInt8 seen;           // seen children bits
    PWUInt8 oct;            // children bits
    PWUInt8 level;          // octree depth level
    PWUInt8 flags;          // flags (0x01 = no children in data)

    PWUInt32 src_offset;    // offset in tile data (for level)
    PWUInt32 idx_children;  // 0 = no children

    PWUInt8 data[ ItemDataSize ];
};

//----------------------------------------------------------------

template<int ItemDataSize>
class PWRenderNodes1
{
public:
    PWRenderNodes1()
    {
        m_nodes = 0;
        m_maxNodes = 0;
        m_lastAlloc = 0;

        m_num_free = 0;
        m_num_used = 0;
    }

    ~PWRenderNodes1()
    {
        Free();
    }

public:
    void Init( PWUInt32 maxNodes )
    {
        Free();
        
        if ( maxNodes < 1 ) maxNodes = 1;
        m_nodes = new SPWRenderNode1<ItemDataSize>[ maxNodes ];

        memset( m_nodes, 0, sizeof(SPWRenderNode1<ItemDataSize>) * maxNodes );

        m_maxNodes = maxNodes;
        m_lastAlloc = 1;
        
        m_num_free = 0;
        m_num_used = 0;

        // NOTE: index 0 not for allocs/frees (could be used as root if required)
    }

    void Free()
    {
        if ( m_nodes ) { delete[] m_nodes; m_nodes = 0; }

        m_maxNodes = 0;
        m_lastAlloc = 0;

        for ( int i = 0; i < 8; ++i )
            m_free_idxs[i].Clear();

        m_num_free = 0;
        m_num_used = 0;
    }

    void Flush()
    {
        m_lastAlloc = m_nodes ? 1 : 0;

        for ( int i = 0; i < 8; ++i )
            m_free_idxs[i].Clear();

        m_num_free = 0;
        m_num_used = 0;
    }

public:
    PWUInt32 AllocNodes( int count )
    {
        // NO ARGS CHECKS!
        //
        // count = 1..8

        if ( !m_free_idxs[ count-1 ].IsEmpty() )
        {
            PWUInt32 idx;
            m_free_idxs[ count-1 ].PopBack( &idx );

            //m_num_free -= count;
            //m_num_used += count;
            return idx;
        }

        if ( m_lastAlloc + count <= m_maxNodes )
        {
            PWUInt32 idx = m_lastAlloc;
            m_lastAlloc += count;

            //m_num_used += count;
            return idx;
        }

        return 0;
    }

    void FreeNodes( PWUInt32 idx, int count )
    {
        // NO ARGS CHECKS!
        //
        // idx   = 1..
        // count = 1..8

        m_free_idxs[ count-1 ].PushBack( idx );
        
        //m_num_used -= count;
        //m_num_free += count;
    }

    void FreeNodesTree( PWUInt32 idx, int count )
    {
        for ( int i = 0; i < count; ++i )
        {
            SPWRenderNode1<ItemDataSize> *pNode = &m_nodes[ idx+i ];

            if ( pNode->idx_children )
            {
                FreeNodesTree( pNode->idx_children, m_lut_byte_counts.lookup[ pNode->oct ] );
                pNode->idx_children = 0; // ensure
            }
        }

        FreeNodes( idx, count );
    }

public:
    SPWRenderNode1<ItemDataSize> *Nodes()
    {
        return m_nodes;
    }

public:
    void GetStat_RenderNodes( int & num_used, int & num_free ) const
    {
        num_used = m_num_used;
        num_free = m_num_free;
    }

private:
    SPWRenderNode1<ItemDataSize> *m_nodes;
    PWUInt32                      m_maxNodes;
    PWUInt32                      m_lastAlloc;
    PWArray<PWUInt32>             m_free_idxs[8];
    PWLUT_ByteCounts              m_lut_byte_counts;

    int m_num_free;
    int m_num_used;
};

//----------------------------------------------------------------

class PWNotSeenList1
{
public:
    PWNotSeenList1()
    {
        Clear();
    }

public:
    void Clear()
    {
        for ( int tidx = 0; tidx < 2; ++tidx )
        {
            STable & table = m_tables[ tidx ];

            memset( table.idx,  0, 32*NotSeenTableCount*4 );
            memset( table.pos,  0, 32*4 );
        }
    }

    void AddNotSeen( int level, PWUInt32 idx_parent, bool tile )
    {
        // NO ARG CHECKS!

        STable & table = m_tables[ tile ? 0 : 1 ];

        PWUInt32 pos = table.pos[ level ];

        ++pos;
        pos &= (NotSeenTableCount-1);

        const int ofs = (level << NotSeenTableShift) | pos;

        table.idx[ ofs   ] = idx_parent;
        table.pos[ level ] = pos;
    }

    void Gather( PWArray<PWUInt32> & idxs_tile, PWArray<PWUInt32> & idxs_nottile )
    {
        idxs_tile.Clear();
        idxs_nottile.Clear();

        for ( int tidx = 0; tidx < 2; ++tidx )
        {
            PWArray<PWUInt32> & dest = (tidx==0) ? idxs_tile : idxs_nottile;

            STable & table = m_tables[ tidx ];

            const PWUInt32 *idxs = &table.idx[0];
            for ( int level = 0; level < 30; ++level )
            {
                PWUInt32 pos = table.pos[ level ];
                for ( int cnt = 0; cnt < NotSeenTableCount; ++cnt )
                {
                    const PWUInt32 idx = idxs[ pos ];
                    if ( !idx )
                        break;

                    dest.PushBack( idx );

                    pos = (pos + (NotSeenTableCount-1)) & (NotSeenTableCount-1);
                }

                idxs += NotSeenTableCount;
            }
        }
    }

private:
    static const int NotSeenTableShift = 12;
    static const int NotSeenTableCount = 1 << NotSeenTableShift;

    struct STable
    {
        PWUInt32 idx[ 32 * NotSeenTableCount ];
        PWUInt32 pos[ 32 ];
    };

    STable m_tables[2];
};

//----------------------------------------------------------------

#endif
