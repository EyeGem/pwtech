// pw_indexset.h
#ifndef pw_indexsetH
#define pw_indexsetH

#include "pw_array.h"

//----------------------------------------------------------------

template<typename T, int ItemsPerBlock>
class PWIndexSet
{
    // NO COPY
private:
    PWIndexSet( const PWIndexSet & );
    void operator = ( const PWIndexSet & );

public:
    PWIndexSet() { Reset(); }
    ~PWIndexSet() { Cleanup(); }

public:
    int MinIndex() const
    {
        return m_minIndex;
    }

    int MaxIndex() const
    {
        return m_maxIndex;
    }

public:
    bool Has( int index ) const
    {
        if ( index < 0 )
            return false;
        
        const int blk = index / ItemsPerBlock;
        if ( blk >= m_blocks.GetCount() )
            return false;
        
        const SBlock *pBlock = m_blocks[ blk ];
        if ( !pBlock )
            return false;

        const int idx = index % ItemsPerBlock;
        const SItem & item = pBlock->items[ idx ];

        if ( item.allocated == 0 )
            return false;

        return true;
    }

    const T *Read( int index ) const
    {
        if ( index < 0 )
            return 0;
        
        const int blk = index / ItemsPerBlock;
        if ( blk >= m_blocks.GetCount() )
            return 0;
        
        const SBlock *pBlock = m_blocks[ blk ];
        if ( !pBlock )
            return 0;

        const int idx = index % ItemsPerBlock;
        const SItem & item = pBlock->items[ idx ];

        if ( item.allocated == 0 )
            return 0;

        return (const T *)item.data;
    }

    T *Modify( int index )
    {
        if ( index < 0 )
            return 0;
        
        const int blk = index / ItemsPerBlock;
        if ( blk >= m_blocks.GetCount() )
            return 0;
        
        SBlock *pBlock = m_blocks[ blk ];
        if ( !pBlock )
            return 0;

        const int idx = index % ItemsPerBlock;
        SItem & item = pBlock->items[ idx ];

        if ( item.allocated == 0 )
            return 0;

        return (T *)item.data;
    }

    T *Write( int index )
    {
        if ( index < 0 )
            return 0;
        
        const int blk = index / ItemsPerBlock;
        if ( blk >= m_blocks.GetCount() )
        {
            m_blocks.Resize( 1+blk, 0 );
        }
        
        SBlock *pBlock = m_blocks[ blk ];
        if ( !pBlock )
        {
            pBlock = new SBlock();

            for ( int i = 0; i < ItemsPerBlock; ++i )
                pBlock->items[i].allocated = 0;

            m_blocks[ blk ] = pBlock;
        }

        const int idx = index % ItemsPerBlock;
        SItem & item = pBlock->items[ idx ];

        if ( item.allocated == 0 )
        {
            new(item.data) T;
            item.allocated = 1;

            if ( m_minIndex < 0 || index < m_minIndex ) m_minIndex = index;
            if ( m_maxIndex < 0 || index > m_maxIndex ) m_maxIndex = index;
        }

        return (T *)item.data;
    }

    bool Remove( int index )
    {
        if ( index < 0 )
            return false;
        
        const int blk = index / ItemsPerBlock;
        if ( blk >= m_blocks.GetCount() )
            return false;
        
        SBlock *pBlock = m_blocks[ blk ];
        if ( !pBlock )
            return false;

        const int idx = index % ItemsPerBlock;
        SItem & item = pBlock->items[ idx ];

        if ( item.allocated == 0 )
            return false;

        ((T *)item.data)->~T();
        item.allocated = 0;

        return true;
    }

public:
    void Clear()
    {
        for ( int i = m_minIndex; i <= m_maxIndex; ++i )
            Remove(i);
    }

    void Cleanup()
    {
        Clear();

        const int count = m_blocks.GetCount();
        for ( int i = 0; i < count; ++i )
        {
            if ( m_blocks[i] )
                delete m_blocks[i];
        }

        m_blocks.Clear();
        Reset();
    }

private:
    void Reset()
    {
        m_minIndex = -1;
        m_maxIndex = -1;
    }

private:
    struct SItem { PWUInt32 data[ ( sizeof(T) - 1 ) / sizeof(PWUInt32) + 1 ]; char allocated; };
    static const int BlockSize = ItemsPerBlock * sizeof(SItem);
    struct SBlock { SItem items[ ItemsPerBlock ]; };
    PWArray<SBlock *> m_blocks;

    int m_minIndex;
    int m_maxIndex;
};

//----------------------------------------------------------------

#endif
