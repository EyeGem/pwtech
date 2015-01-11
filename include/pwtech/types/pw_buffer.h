// pw_buffer.h
#ifndef pw_bufferH
#define pw_bufferH

#include "pw_ints.h"
#include "pw_array.h"

#include <memory.h>
#include <string.h>

//----------------------------------------------------------------

class PWBuffer
{
public:
    PWBuffer()
    {
        m_offset = BlockSize;
    }

    ~PWBuffer()
    {
        for ( int i = 0; i < m_blocks.GetCount(); ++i )
            delete[] m_blocks[i];

        m_blocks.Clear();
    }

public:
    PWBuffer( const PWBuffer & from )
    {
        MoveFrom( from );
    }

    void operator = ( const PWBuffer & from )
    {
        MoveFrom( from );
    }

private:
    void MoveFrom( const PWBuffer & from )
    {
        // MOVE ON COPY
        PWBuffer *pFrom = const_cast<PWBuffer *>( &from );

        m_blocks = pFrom->m_blocks;
        m_offset = pFrom->m_offset;

        pFrom->m_blocks.Clear();
        pFrom->m_offset = BlockSize;
    }

public:
    void Write( const void *src, PWUInt32 size )
    {
        // NO ARGS CHECKS!

        const char *rd = (const char *)src;
        while ( size > 0 )
        {
            if ( m_offset >= BlockSize )
            {
                m_blocks.PushBack( new char[BlockSize] );
                m_offset = 0;
            }

            PWUInt32 sizeWrite = BlockSize - m_offset;
            if ( sizeWrite > size )
                sizeWrite = size;

            memcpy( (void *)( m_blocks[ m_blocks.GetCount()-1 ] + m_offset ), rd, sizeWrite );

            m_offset += sizeWrite;
            size -= sizeWrite;
            rd += sizeWrite;
        }
    }

public:
    PWUInt32 GetBlocksCount() const
    {
        return m_blocks.GetCount();
    }

    const char *GetBlockData( int block_index, PWUInt32 & dataSize ) const
    {
        // NO ARGS CHECKS!

        if ( block_index + 1 == m_blocks.GetCount() )
            dataSize = m_offset;
        else
            dataSize = BlockSize;

        return m_blocks[ block_index ];
    }

    PWUInt32 GetTotalSize() const
    {
        PWUInt32 sz = 0;
        PWUInt32 block_sz;

        for ( int i = 0; i < m_blocks.GetCount(); ++i )
        {
            GetBlockData( i, block_sz );
            sz += block_sz;
        }

        return sz;
    }

private:
    static const PWUInt32 BlockSize = 256 * 1024; // 256K

    PWArray<const char *> m_blocks;
    PWUInt32              m_offset;
};

//----------------------------------------------------------------

#endif
