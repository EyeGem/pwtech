// pw_rendernodes2.h
#ifndef pw_rendernodes2H
#define pw_rendernodes2H

#include "../../types/pw_ints.h"

//----------------------------------------------------------------

template<int ItemDataSize>
struct SPWRenderNode2
{
    PWUInt32 src;          // offset in tile data (for level)
    PWUInt32 idx_children; // 0 = no children

    PWUInt8  level_flags;  // level(0..31) + flag (0x80 = children open)
    PWUInt8  oct;          // children bits
    PWUInt16 tile;         // tile (0 = not map tile node)

    PWUInt8  data[ ItemDataSize ];
};

//----------------------------------------------------------------

template<int ItemDataSize>
class PWRenderNodes2
{
public:
    typedef SPWRenderNode2<ItemDataSize> TNode;

public:
    PWRenderNodes2()
    {
        m_nodes1 = 0;
        m_nodes2 = 0;
        m_max_nodes = 0;

        m_nodes_read = 0;
        m_nodes_write = 0;
        m_write_pos = 0;
    }

    ~PWRenderNodes2()
    {
        Free();
    }

public:
    void Init( PWUInt32 max_nodes )
    {
        Free();
        
        if ( max_nodes < 1 )
            max_nodes = 1;

        m_nodes1 = new TNode[ max_nodes ];
        m_nodes2 = new TNode[ max_nodes ];
        m_max_nodes = max_nodes;

        Clear();

        m_nodes_read  = m_nodes1;
        m_nodes_write = m_nodes2;
    }

    void Free()
    {
        if ( m_nodes1 ) { delete[] m_nodes1; m_nodes1 = 0; }
        if ( m_nodes2 ) { delete[] m_nodes2; m_nodes2 = 0; }
        m_max_nodes = 0;

        m_nodes_read = 0;
        m_nodes_write = 0;
        m_write_pos = 0;
    }

public:
    void Clear()
    {
        if ( m_nodes1 )
            m_nodes1->src = 0;

        if ( m_nodes2 )
            m_nodes2->src = 0;

        m_write_pos = 0;
    }

public:
    const TNode *ReadNode( PWUInt32 idx ) const
    {
        // NO ARGS CHECKS!

        return &m_nodes_read[ idx ];
    }

    TNode *WriteNodes( PWUInt32 & out_idx, int count )
    {
        // NO ARGS CHECKS!

        if ( m_write_pos + count > m_max_nodes )
        {
            out_idx = 0;
            return 0;
        }

        out_idx = m_write_pos;
        TNode *pNodes = &m_nodes_write[ m_write_pos ];
        
        m_write_pos += count;
        return pNodes;
    }

    TNode *ModifyNode( PWUInt32 idx )
    {
        // NO ARGS CHECKS!

        return &m_nodes_write[ idx ];
    }

    void Swap()
    {
        if ( m_nodes_read == m_nodes1 )
        {
            m_nodes_read  = m_nodes2;
            m_nodes_write = m_nodes1;
        }
        else
        {
            m_nodes_read  = m_nodes1;
            m_nodes_write = m_nodes2;
        }

        m_write_pos = 0;
    }

private:
    SPWRenderNode2<ItemDataSize> *m_nodes1;
    SPWRenderNode2<ItemDataSize> *m_nodes2;
    PWUInt32                      m_max_nodes;

    SPWRenderNode2<ItemDataSize> *m_nodes_read;
    SPWRenderNode2<ItemDataSize> *m_nodes_write;
    PWUInt32                      m_write_pos;
};

//----------------------------------------------------------------

#endif
