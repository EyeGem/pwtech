// pw_tilesmap.h
#ifndef pw_tilesmapH
#define pw_tilesmapH

#include "pw_lookups.h"

#include "wraps/pw_file.h"

#include "types/pw_ints.h"
#include "types/pw_array.h"

#include <memory.h>
#include <string.h>

//----------------------------------------------------------------

typedef void (*TPWMapMixFunc)( void *dest, const void *src, int count, int level );

//----------------------------------------------------------------

template<int ItemDataSize>
class PWTilesMap
{
private: // NO COPY
    PWTilesMap( const PWTilesMap & );
    void operator = ( const PWTilesMap & );

public:
    PWTilesMap();
    ~PWTilesMap();

public:
    void SetTileData( int tile, const void *data );

public:
    void Clear();
    bool Load( IPWFile *pFile );
    bool Save( IPWFile *pFile );

public:
    int GetTile( int x, int y, int z );
    void SetTile( int x, int y, int z, int tile, TPWMapMixFunc func = 0 );
    bool UpdateAll( TPWMapMixFunc func );

public:
    PWUInt32 FindNode( int x, int y, int z );
    bool FindNodePath( PWArray<PWUInt32> & out_path, int x, int y, int z );
    void FlushNodes( int x, int y, int z );
    void FlushAllNodes();

public:
    struct SNode
    {
        PWUInt8  extra; // extra value
        PWUInt8  oct;   // 0 = no children
        PWUInt16 tile;  // 0 = empty tile

        PWUInt32 idx_children;
        
        PWUInt8 data[ ItemDataSize ];
    };

    PWUInt32 GetNodesCount() const
    {
        return m_alloc_block * BlockCount + m_alloc_idx;
    }

    SNode *GetNode( PWUInt32 idx )
    {
        // NO ARGS CHECKS!

        PWUInt32 blk = idx >> BlockShift;
        return &m_blocks[ blk ]->nodes[ idx & (BlockCount-1) ];
    }

    PWUInt32 GetRoot( PWInt32 & dx, PWInt32 & dy, PWInt32 & dz, PWUInt32 & size ) const
    {
        dx   = m_root_dx;
        dy   = m_root_dy;
        dz   = m_root_dz;
        size = m_root_size;

        return m_root_idx;
    }

private:
    static const int     BlockShift = 16;
    static const PWInt32 BlockCount = 1 << BlockShift;

    struct SBlock
    {
        SNode nodes[ BlockCount ];
    };

    PWUInt8          *m_tiles_data;
    PWArray<SBlock *> m_blocks;
    PWUInt32          m_root_idx;
    PWInt32           m_root_dx;
    PWInt32           m_root_dy;
    PWInt32           m_root_dz;
    PWUInt32          m_root_size;
    PWArray<PWUInt32> m_free_idxs[8];
    PWInt32           m_alloc_block;
    PWInt32           m_alloc_idx;

    PWLUT_ByteCounts  m_lut_byte_counts;
    PWLUT_OctOffsets  m_lut_oct_offsets;
    PWLUT_OctIndexes  m_lut_oct_indexes;
    
    PWUInt8           m_mixbuf[ ItemDataSize * 8 ];
    PWArray<SNode *>  m_parents;

private:
    SNode *AllocNodes( PWUInt32 & out_idx, int count )
    {
        // ASSUMED count >= 1 AND count <= 8

        PWArray<PWUInt32> & free_idxs = m_free_idxs[ count-1 ];
        if ( !free_idxs.IsEmpty() )
        {
            free_idxs.PopBack( &out_idx );

            return GetNode( out_idx );
        }

        if ( m_alloc_idx > ( BlockCount - count ) )
        {
            if ( m_alloc_idx < BlockCount )
                m_free_idxs[ (BlockCount - m_alloc_idx)-1 ].PushBack( (( m_alloc_block * BlockCount ) + m_alloc_idx) );

            m_blocks.PushBack( new SBlock() );
            ++m_alloc_block;
            m_alloc_idx = 0;
        }

        out_idx = ( m_alloc_block * BlockCount ) + m_alloc_idx;

        m_alloc_idx += count;
        return GetNode( out_idx );
    }

    void FreeNodes( PWUInt32 idx, int count )
    {
        // ASSUMED count >= 1 AND count <= 8

        m_free_idxs[ count-1 ].PushBack( idx );
    }

private:
    int UpdateRecursive( SNode *pNode, int & nodes, TPWMapMixFunc func );
};

//----------------------------------------------------------------

template<int ItemDataSize> PWTilesMap<ItemDataSize>::PWTilesMap()
{
    m_tiles_data  = 0;
    m_root_idx    = 1;
    m_root_dx     = 0;
    m_root_dy     = 0;
    m_root_dz     = 0;
    m_root_size   = 1;
    m_alloc_block = 0;
    m_alloc_idx   = 2;

    m_blocks.PushBack( new SBlock() );

    SNode & rootNode = m_blocks[0]->nodes[ m_root_idx ];
    memset( &rootNode, 0, sizeof(SNode) );
}

//----------------------------------------------------------------

template<int ItemDataSize> PWTilesMap<ItemDataSize>::~PWTilesMap()
{
    if ( m_tiles_data )
    {
        free( m_tiles_data );
        m_tiles_data = 0;
    }

    for ( int i = 0; i < m_blocks.GetCount(); ++i )
    {
        if ( m_blocks[i] )
        {
            delete[] m_blocks[i];
            m_blocks[i] = 0;
        }
    }
    m_blocks.Clear();
}

//----------------------------------------------------------------

template<int ItemDataSize>
void PWTilesMap<ItemDataSize>::SetTileData( int tile, const void *data )
{
    if ( tile < 0 || tile >= 65536 )
        return;

    if ( !data )
        return;

    if ( !m_tiles_data )
    {
        m_tiles_data = (PWUInt8 *)malloc( 65536 * ItemDataSize );
        memset( m_tiles_data, 0, 65536 * ItemDataSize );
    }

    memcpy( m_tiles_data + ((PWUInt32)tile) * ItemDataSize, data, ItemDataSize );
}

//----------------------------------------------------------------

template<int ItemDataSize>
void PWTilesMap<ItemDataSize>::Clear()
{
    for ( int i = 1; i < m_blocks.GetCount(); ++i )
    {
        if ( m_blocks[i] )
        {
            delete[] m_blocks[i];
            m_blocks[i] = 0;
        }
    }
    m_blocks.Resize(1);

    m_root_idx    = 1;
    m_root_dx     = 0;
    m_root_dy     = 0;
    m_root_dz     = 0;
    m_root_size   = 1;
    m_alloc_block = 0;
    m_alloc_idx   = 2;

    for ( int i = 0; i < 8; ++i )
        m_free_idxs[i].Clear();

    SNode & rootNode = m_blocks[0]->nodes[ m_root_idx ];
    memset( &rootNode, 0, sizeof(SNode) );
}

//----------------------------------------------------------------

template<int ItemDataSize>
bool PWTilesMap<ItemDataSize>::Load( IPWFile *pFile )
{
    Clear();

    if ( !pFile )
        return false;

    char buf[8];
    if ( !pFile->Read( buf, 8 ) ) return false;

    if ( buf[0] != 'P' ) return false;
    if ( buf[1] != 'W' ) return false;
    if ( buf[2] != 'T' ) return false;
    if ( buf[3] != 'M' ) return false;
    if ( buf[4] != 'A' ) return false;
    if ( buf[5] != 'P' ) return false;
    if ( buf[6] != '0' ) return false;
    if ( buf[7] != '1' ) return false;

    PWUInt8 itemDataSize = 0;
    PWUInt8 nodeSize     = 0;
    PWUInt32 nodesCount  = 0;

    if ( !pFile->Read( buf, 1 ) ) return false;
    if ( !pFile->Read( &itemDataSize, 1 ) ) return false;
    if ( !pFile->Read( &nodeSize, 1 ) ) return false;
    if ( !pFile->Read( buf, 1 ) ) return false;
    if ( !pFile->Read( &nodesCount, 4 ) ) return false;

    if ( itemDataSize != ItemDataSize )
        return false;

    if ( nodeSize != sizeof(SNode) )
        return false;

    PWInt32  mdx = 0;
    PWInt32  mdy = 0;
    PWInt32  mdz = 0;
    PWUInt32 msz = 0;
    PWUInt32 root_idx = 0;

    if ( !pFile->Read( &mdx, 4 ) ) return false;
    if ( !pFile->Read( &mdy, 4 ) ) return false;
    if ( !pFile->Read( &mdz, 4 ) ) return false;
    if ( !pFile->Read( &msz, 4 ) ) return false;

    if ( !pFile->Read( &root_idx, 4 ) ) return false;
    if ( !pFile->Read( buf, 4 ) ) return false;
    if ( !pFile->Read( buf, 4 ) ) return false;
    if ( !pFile->Read( buf, 4 ) ) return false;

    if ( msz <= 0 )
        return false;

    if ( root_idx == 0 || root_idx >= nodesCount )
        return false;

    for ( int i = 0; i < 8; ++i )
    {
        int count;
        if ( !pFile->Read( &count, 4 ) )
            return false;

        if ( count < 0 || (PWUInt32)count >= nodesCount )
            return false;

        m_free_idxs[i].Resize( count );
    }

    for ( int i = 0; i < 8; ++i )
    {
        const int count = m_free_idxs[i].GetCount();
        if ( count <= 0 )
            continue;

        if ( !pFile->Read( &(m_free_idxs[i][0]), 4*count ) )
            return false;

        for ( int j = 0; j < count; ++j )
            if ( m_free_idxs[i][j] == 0 || m_free_idxs[i][j] >= nodesCount )
                return false;
    }

    m_alloc_block = nodesCount / BlockCount;
    m_alloc_idx   = nodesCount % BlockCount;
    
    m_blocks.Resize( m_alloc_block + 1 );
    for ( int blk = 1; blk <= m_alloc_block; ++blk )
        m_blocks[ blk ] = new SBlock();

    for ( int blk = 0; blk <= m_alloc_block; ++blk )
    {
        memset( &m_blocks[ blk ]->nodes[0], 0, sizeof(SNode) * BlockCount );
    }

    for ( int blk = 0; blk <= m_alloc_block; ++blk )
    {
        const int count = ( blk < m_alloc_block ) ? BlockCount : m_alloc_idx;
        
        SNode *pNodes = &m_blocks[ blk ]->nodes[0];
        if ( !pFile->Read( pNodes, sizeof(SNode) * count ) )
        {
            Clear();
            return false;
        }
    }

    m_root_dx   = mdx;
    m_root_dy   = mdy;
    m_root_dz   = mdz;
    m_root_size = msz;
    m_root_idx  = root_idx;

    return true;
}

//----------------------------------------------------------------

template<int ItemDataSize>
bool PWTilesMap<ItemDataSize>::Save( IPWFile *pFile )
{
    if ( !pFile || !pFile->CanWrite() )
        return false;

    FlushAllNodes();

    char zbuf[8];
    memset( zbuf, 0, 8 );

    const PWUInt8 itemDataSize = ItemDataSize;
    const PWUInt8 nodesize = sizeof(SNode);
    const PWUInt32 nodesCount = m_alloc_block * BlockCount + m_alloc_idx;

    if ( !pFile->Write( "PWTMAP01", 8 ) ) return false;
    if ( !pFile->Write( &zbuf[0], 1 ) ) return false;
    if ( !pFile->Write( &itemDataSize, 1 ) ) return false;
    if ( !pFile->Write( &nodesize, 1 ) ) return false;
    if ( !pFile->Write( &zbuf[0], 1 ) ) return false;
    if ( !pFile->Write( &nodesCount, 4 ) ) return false;

    if ( !pFile->Write( &m_root_dx, 4 ) ) return false;
    if ( !pFile->Write( &m_root_dy, 4 ) ) return false;
    if ( !pFile->Write( &m_root_dz, 4 ) ) return false;
    if ( !pFile->Write( &m_root_size, 4 ) ) return false;

    if ( !pFile->Write( &m_root_idx, 4 ) ) return false;
    if ( !pFile->Write( &zbuf[0], 4 ) ) return false;
    if ( !pFile->Write( &zbuf[0], 4 ) ) return false;
    if ( !pFile->Write( &zbuf[0], 4 ) ) return false;

    for ( int i = 0; i < 8; ++i )
    {
        int count = m_free_idxs[i].GetCount();
        if ( !pFile->Write( &count, 4 ) )
            return false;
    }

    for ( int i = 0; i < 8; ++i )
    {
        int count = m_free_idxs[i].GetCount();
        if ( count > 0 )
        {
            if ( !pFile->Write( &(m_free_idxs[i][0]), 4*count ) )
                return false;
        }
    }

    for ( int blk = 0; blk < m_blocks.GetCount(); ++blk )
    {
        const int count = ( blk < m_alloc_block ) ? BlockCount : m_alloc_idx;
        
        SNode *pNodes = &m_blocks[ blk ]->nodes[0];
        if ( !pFile->Write( pNodes, sizeof(SNode) * count ) )
            return false;
    }

    return true;
}

//----------------------------------------------------------------

// Format:
//    
// 8 "PWTMAP01"
// 1 0
// 1 itemdatasize
// 1 nodesize
// 1 (0)
// 4 nodes count
//    
// 4 map_dx
// 4 map_dy
// 4 map_dz
// 4 map_size
//
// 4 root_idx
// 4 (0)
// 4 (0)
// 4 (0)
//
// 4*8 free_count[8]
// 4*free_count[0] free_1[]
// 4*free_count[1] free_2[]
// 4*free_count[2] free_3[]
// 4*free_count[3] free_4[]
// 4*free_count[4] free_5[]
// 4*free_count[5] free_6[]
// 4*free_count[6] free_7[]
// 4*free_count[7] free_8[]
//
// (nodes count * nodesize) nodes

//----------------------------------------------------------------

template<int ItemDataSize>
int PWTilesMap<ItemDataSize>::GetTile( int x, int y, int z )
{
    x += m_root_dx;
    y += m_root_dy;
    z += m_root_dz;

    if ( x < 0 || (PWUInt32)x >= m_root_size ) return 0;
    if ( y < 0 || (PWUInt32)y >= m_root_size ) return 0;
    if ( z < 0 || (PWUInt32)z >= m_root_size ) return 0;

    PWUInt32 ds = m_root_size >> 1;

    x -= ds;
    y -= ds;
    z -= ds;

    SNode *pNode = GetNode( m_root_idx );
    while ( ds )
    {    
        if ( !pNode->oct )
            return 0;

        ds >>= 1;

        int octidx = 0;
        if ( x < 0 ) { x += ds; } else { x -= ds; octidx |= 1; }
        if ( y < 0 ) { y += ds; } else { y -= ds; octidx |= 2; }
        if ( z < 0 ) { z += ds; } else { z -= ds; octidx |= 4; }

        const int child_offset = m_lut_oct_offsets.lookup[ (((int)pNode->oct) << 3) | octidx ];
        if ( child_offset == 0xFF )
            return 0;

        pNode = GetNode( pNode->idx_children + child_offset );
    }

    return pNode->tile;
}

//----------------------------------------------------------------

template<int ItemDataSize>
void PWTilesMap<ItemDataSize>::SetTile( int x, int y, int z, int tile, TPWMapMixFunc func )
{
    if ( !tile )
    {
        x += m_root_dx;
        y += m_root_dy;
        z += m_root_dz;

        if ( x < 0 || (PWUInt32)x >= m_root_size ) return;
        if ( y < 0 || (PWUInt32)y >= m_root_size ) return;
        if ( z < 0 || (PWUInt32)z >= m_root_size ) return;

        if ( m_root_size == 1 && GetNode( m_root_idx )->tile != 0 )
        {
            m_root_dx = 0;
            m_root_dy = 0;
            m_root_dz = 0;

            SNode *pRoot = GetNode( m_root_idx );

            pRoot->tile = 0;
            memset( pRoot->data, 0, ItemDataSize );
            return;
        }

        PWUInt32 ds = m_root_size >> 1;

        x -= ds;
        y -= ds;
        z -= ds;

        m_parents.Clear();
        SNode *pNode = GetNode( m_root_idx );
        while ( ds )
        {    
            if ( !pNode->oct )
                return;

            ds >>= 1;

            int octidx = 0;
            if ( x < 0 ) { x += ds; } else { x -= ds; octidx |= 1; }
            if ( y < 0 ) { y += ds; } else { y -= ds; octidx |= 2; }
            if ( z < 0 ) { z += ds; } else { z -= ds; octidx |= 4; }

            const int child_offset = m_lut_oct_offsets.lookup[ (((int)pNode->oct) << 3) | octidx ];
            if ( child_offset == 0xFF )
                return;

            m_parents.PushBack( pNode );
            pNode = GetNode( pNode->idx_children + child_offset );
        }

        pNode->tile = 0;

        int level = -1;
        while ( !m_parents.IsEmpty() )
        {
            m_parents.PopBack( &pNode );
            ++level;
            
            const int count = m_lut_byte_counts.lookup[ pNode->oct ];

            SNode *pChildren = GetNode( pNode->idx_children );

            int new_count = 0;
            int new_oct = 0;
            for ( int i = 0; i < count; ++i )
            {
                SNode *pChild = (pChildren+i);

                if ( pChild->tile )
                {
                    ++new_count;
                    new_oct |= 1 << m_lut_oct_indexes.lookup[ (pNode->oct << 3) | i ];
                }
            }

            if ( new_count <= 0 )
            {
                FreeNodes( pNode->idx_children, count );

                pNode->idx_children = 0;
                pNode->oct = 0;
                pNode->tile = 0;
                pNode->extra = 0;

                continue;
            }

            PWUInt32 new_idx;
            SNode *pNewChildren = AllocNodes( new_idx, new_count );
            if ( !pNewChildren )
                return;

            int new_ofs = 0;
            for ( int i = 0; i < count; ++i )
            {
                SNode *pChild = (pChildren+i);

                if ( pChild->tile )
                {
                    SNode *pNewChild = (pNewChildren+new_ofs);
                    ++new_ofs;

                    memcpy( pNewChild, pChild, sizeof(SNode) );
                }
            }

            FreeNodes( pNode->idx_children, count );            

            pNode->idx_children = new_idx;
            pNode->oct = new_oct;
            pNode->extra = 0;

            while ( pNode )
            {                   
                const int mix_children_count = m_lut_byte_counts.lookup[ pNode->oct ];

                SNode *pMixChildren = GetNode( pNode->idx_children );

                int any_tile = 0;

                if ( func )
                {
                    int mix_count = 0;
                    for ( int i = 0; i < mix_children_count; ++i )
                    {
                        SNode *pMixChild = (pMixChildren+i);

                        //if ( pMixChild->tile )
                        {
                            memcpy( m_mixbuf + mix_count * ItemDataSize, pMixChild->data, ItemDataSize );
                            any_tile = pMixChild->tile;
                            ++mix_count;
                        }
                    }

                    func( pNode->data, m_mixbuf, mix_count, level );
                }
                else
                {
                    any_tile = pMixChildren->tile;
                }

                pNode->tile = any_tile;

                if ( m_parents.IsEmpty() )
                    break;

                m_parents.PopBack( &pNode );
                ++level;
            }

            break;
        }

        return;
    }

    if ( m_root_size == 1 && GetNode( m_root_idx )->tile == 0 )
    {
        m_root_dx = -x;
        m_root_dy = -y;
        m_root_dz = -z;

        SNode *pRoot = GetNode( m_root_idx );

        pRoot->tile = (PWUInt16)tile;

        if ( m_tiles_data )
        {
            memcpy( pRoot->data, m_tiles_data + ItemDataSize * tile, ItemDataSize );
        }

        return;
    }

    x += m_root_dx;
    y += m_root_dy;
    z += m_root_dz;

    while ( x < 0 || (PWUInt32)x >= m_root_size ||
            y < 0 || (PWUInt32)y >= m_root_size ||
            z < 0 || (PWUInt32)z >= m_root_size )
    {
        int octidx = 0;
        if ( x < 0 ) { m_root_dx += m_root_size; x += m_root_size; octidx |= 1; }
        if ( y < 0 ) { m_root_dy += m_root_size; y += m_root_size; octidx |= 2; }
        if ( z < 0 ) { m_root_dz += m_root_size; z += m_root_size; octidx |= 4; }
        m_root_size <<= 1;

        SNode *pOldRoot = GetNode( m_root_idx );

        PWUInt32 new_idx;
        SNode *pNewRoot = AllocNodes( new_idx, 1 );
        if ( !pNewRoot )
            return;

        pNewRoot->idx_children = m_root_idx;
        pNewRoot->oct = (PWUInt8)( 1 << octidx );
        pNewRoot->extra = 0;
        pNewRoot->tile = pOldRoot->tile;
        memcpy( pNewRoot->data, pOldRoot->data, ItemDataSize );

        m_root_idx = new_idx;
    }

    PWUInt32 ds = m_root_size >> 1;

    x -= ds;
    y -= ds;
    z -= ds;

    m_parents.Clear();
    PWInt32 idx = m_root_idx;
    SNode *pNode = GetNode( idx );
    while ( ds )
    {    
        ds >>= 1;

        int octidx = 0;
        if ( x < 0 ) { x += ds; } else { x -= ds; octidx |= 1; }
        if ( y < 0 ) { y += ds; } else { y -= ds; octidx |= 2; }
        if ( z < 0 ) { z += ds; } else { z -= ds; octidx |= 4; }

        if ( !pNode->oct )
        {
            SNode *pChild = AllocNodes( pNode->idx_children, 1 );
            if ( !pChild )
                return;

            pChild->idx_children = 0;
            pChild->oct = 0;
            pChild->extra = 0;
            pChild->tile = 0;
            memset( pChild->data, 0, ItemDataSize );

            pNode->oct = (PWUInt8)( 1 << octidx );
        }
        else if ( (pNode->oct & (1 << octidx) ) == 0 )
        {
            SNode *pOldChildren = GetNode( pNode->idx_children );

            const int count = m_lut_byte_counts.lookup[ pNode->oct ];

            PWUInt32 new_idx;
            SNode *pChildren = AllocNodes( new_idx, count+1 );
            if ( !pChildren )
                return;

            pNode->oct |= (PWUInt8)( 1 << octidx );

            int new_ofs = 0;
            int old_ofs = 0;
            for ( int i = 0; i < 8; ++i )
            {
                if ( pNode->oct & (1<<i) )
                {
                    SNode *pChild = pChildren + new_ofs;
                    ++new_ofs;

                    if ( i == octidx )
                    {
                        pChild->idx_children = 0;
                        pChild->oct = 0;
                        pChild->extra = 0;
                        pChild->tile = 0;
                        memset( pChild->data, 0, ItemDataSize );
                    }
                    else
                    {
                        memcpy( pChild, pOldChildren + old_ofs, sizeof(SNode) );
                        ++old_ofs;
                    }
                }
            }

            FreeNodes( pNode->idx_children, count );

            pNode->idx_children = new_idx;
        }

        const int child_offset = m_lut_oct_offsets.lookup[ (((int)pNode->oct) << 3) | octidx ];
        if ( child_offset == 0xFF )
            return; // should not happen

        m_parents.PushBack( pNode );
        idx = pNode->idx_children + child_offset;
        pNode = GetNode( idx );
    }

    pNode->tile = (PWUInt16)tile;

    if ( m_tiles_data )
    {
        memcpy( pNode->data, m_tiles_data + ItemDataSize * tile, ItemDataSize );
    }

    int level = -1;
    while ( !m_parents.IsEmpty() )
    {
        m_parents.PopBack( &pNode );
        ++level;

        const int count = m_lut_byte_counts.lookup[ pNode->oct ];

        SNode *pMixChildren = GetNode( pNode->idx_children );

        int any_tile = 0;

        if ( func )
        {
            int mix_count = 0;
            for ( int i = 0; i < count; ++i )
            {
                SNode *pMixChild = (pMixChildren+i);

                //if ( pMixChild->tile )
                {
                    memcpy( m_mixbuf + mix_count * ItemDataSize, pMixChild->data, ItemDataSize );
                    any_tile = pMixChild->tile;
                    ++mix_count;
                }
            }

            func( pNode->data, m_mixbuf, mix_count, level );
        }
        else
        {
            any_tile = pMixChildren->tile;
        }

        pNode->tile = any_tile;
    }
}

//----------------------------------------------------------------

template<int ItemDataSize>
bool PWTilesMap<ItemDataSize>::UpdateAll( TPWMapMixFunc func )
{
    if ( !func )
        return false;

    if ( !m_tiles_data )
        return false;

    int nodes = 0;
    UpdateRecursive( GetNode( m_root_idx ), nodes, func );

    return true;
}

//----------------------------------------------------------------

template<int ItemDataSize>
int PWTilesMap<ItemDataSize>::UpdateRecursive( SNode *pNode, int & nodes, TPWMapMixFunc func )
{
    ++nodes;

    if ( !pNode->oct )
    {
        memcpy( pNode->data, &m_tiles_data[ ItemDataSize * pNode->tile ], ItemDataSize );
        return 0;
    }

    PWUInt8 mixbuf[ ItemDataSize * 8 ];

    const int count = m_lut_byte_counts.lookup[ pNode->oct ];
    SNode *pChildren = GetNode( pNode->idx_children );
    PWUInt8 *wr = mixbuf;
    int mix_count = 0;
    int mixlevel = -1;
    for ( int i = 0; i < count; ++i )
    {
        SNode *pChild = (pChildren+i);
        //if ( !pChild->tile )
        //    continue;

        mixlevel = UpdateRecursive( pChild, nodes, func );

        memcpy( wr, pChild->data, ItemDataSize );
        wr += ItemDataSize;
        ++mix_count;
    }
    
    func( pNode->data, mixbuf, mix_count, mixlevel );

    return mixlevel + 1;
}

//----------------------------------------------------------------

template<int ItemDataSize>
PWUInt32 PWTilesMap<ItemDataSize>::FindNode( int x, int y, int z )
{
    x += m_root_dx;
    y += m_root_dy;
    z += m_root_dz;

    if ( x < 0 || (PWUInt32)x >= m_root_size ) return 0;
    if ( y < 0 || (PWUInt32)y >= m_root_size ) return 0;
    if ( z < 0 || (PWUInt32)z >= m_root_size ) return 0;

    PWUInt32 ds = m_root_size >> 1;

    x -= ds;
    y -= ds;
    z -= ds;

    PWUInt32 idx = m_root_idx;
    SNode *pNode = GetNode( idx );
    while ( ds )
    {   
        if ( !pNode->oct )
            return 0;

        ds >>= 1;

        int octidx = 0;
        if ( x < 0 ) { x += ds; } else { x -= ds; octidx |= 1; }
        if ( y < 0 ) { y += ds; } else { y -= ds; octidx |= 2; }
        if ( z < 0 ) { z += ds; } else { z -= ds; octidx |= 4; }

        const int child_offset = m_lut_oct_offsets.lookup[ (((int)pNode->oct) << 3) | octidx ];
        if ( child_offset == 0xFF )
            return 0;

        idx = pNode->idx_children + child_offset;
        pNode = GetNode( idx );
    }

    return idx;
}

//----------------------------------------------------------------

template<int ItemDataSize>
bool PWTilesMap<ItemDataSize>::FindNodePath( PWArray<PWUInt32> & out_path, int x, int y, int z )
{
    out_path.Clear();

    x += m_root_dx;
    y += m_root_dy;
    z += m_root_dz;

    if ( x < 0 || (PWUInt32)x >= m_root_size ) return false;
    if ( y < 0 || (PWUInt32)y >= m_root_size ) return false;
    if ( z < 0 || (PWUInt32)z >= m_root_size ) return false;

    PWUInt32 ds = m_root_size >> 1;

    x -= ds;
    y -= ds;
    z -= ds;

    PWUInt32 idx = m_root_idx;
    SNode *pNode = GetNode( idx );
    while ( ds )
    {   
        if ( !pNode->oct )
            return false;

        ds >>= 1;

        int octidx = 0;
        if ( x < 0 ) { x += ds; } else { x -= ds; octidx |= 1; }
        if ( y < 0 ) { y += ds; } else { y -= ds; octidx |= 2; }
        if ( z < 0 ) { z += ds; } else { z -= ds; octidx |= 4; }

        const int child_offset = m_lut_oct_offsets.lookup[ (((int)pNode->oct) << 3) | octidx ];
        if ( child_offset == 0xFF )
            return false;

        out_path.PushBack( idx );

        idx = pNode->idx_children + child_offset;
        pNode = GetNode( idx );
    }

    return true;
}

//----------------------------------------------------------------

template<int ItemDataSize>
void PWTilesMap<ItemDataSize>::FlushNodes( int x, int y, int z )
{
    x += m_root_dx;
    y += m_root_dy;
    z += m_root_dz;

    if ( x < 0 || (PWUInt32)x >= m_root_size ) return;
    if ( y < 0 || (PWUInt32)y >= m_root_size ) return;
    if ( z < 0 || (PWUInt32)z >= m_root_size ) return;

    PWUInt32 ds = m_root_size >> 1;

    x -= ds;
    y -= ds;
    z -= ds;

    SNode *pNode = GetNode( m_root_idx );
    while ( ds )
    {   
        pNode->extra = 0;

        if ( !pNode->oct )
            return;

        ds >>= 1;

        int octidx = 0;
        if ( x < 0 ) { x += ds; } else { x -= ds; octidx |= 1; }
        if ( y < 0 ) { y += ds; } else { y -= ds; octidx |= 2; }
        if ( z < 0 ) { z += ds; } else { z -= ds; octidx |= 4; }

        const int child_offset = m_lut_oct_offsets.lookup[ (((int)pNode->oct) << 3) | octidx ];
        if ( child_offset == 0xFF )
            return;

        pNode = GetNode( pNode->idx_children + child_offset );
    }

    pNode->extra = 0;

    if ( !pNode->oct )
        pNode->idx_children = 0;
}

//----------------------------------------------------------------

template<int ItemDataSize>
void PWTilesMap<ItemDataSize>::FlushAllNodes()
{
    const PWUInt32 nodesCount = GetNodesCount();
    for ( PWUInt32 node_idx = 0; node_idx < nodesCount; ++node_idx )
    {
        SNode *pNode = GetNode( node_idx );

        pNode->extra = 0;

        if ( !pNode->oct )
            pNode->idx_children = 0;
    }
}

//----------------------------------------------------------------

#endif
