// pw_array.h
#ifndef pw_arrayH
#define pw_arrayH

#include <new>

//----------------------------------------------------------------

template<typename TData>
class PWArray
{
public:
    PWArray() { Reset(); }
    ~PWArray() { Cleanup(); }

    PWArray( const PWArray & from ) { Reset(); Copy(from); }
    void operator = ( const PWArray & from ) { Copy(from); }

public:
    bool IsEmpty() const { return m_count <= 0; }
    int GetCount() const { return m_count; }

    TData & operator[]( int index ) { return ((TData*)m_pData)[ index ]; }
    const TData & operator[]( int index ) const { return ((const TData*)m_pData)[ index ]; }

public:
    void Clear()
    {
        TData *ptr = (TData *)m_pData;
        for ( int i = 0; i < m_count; ++i )
        {
            ptr->~TData();
            ++ptr;
        }

        m_count = 0;
    }

    void Resize( int newCount, const TData & value )
    {
        const int wasCount = m_count;
        
        Resize( newCount );

        TData *ptr = (TData *)m_pData + wasCount;
        for ( int i = wasCount; i < m_count; ++i )
        {
            *ptr++ = value;
        }
    }

    void Resize( int newCount )
    {
        if ( newCount < 0 )
            newCount = 0;

        if ( newCount <= m_count )
        {
            TData *ptr = (TData *)m_pData + newCount;
            for ( int i = newCount; i < m_count; ++i )
            {
                ptr->~TData();
                ++ptr;
            }

            m_count = newCount;
            return;
        }

        if ( newCount > m_total )
        {
            const int fixedGrow = 65536;
            const int tryGrow = m_total * sizeof(TData);
            const int tryTotal = m_total + (( tryGrow <= fixedGrow ) ? m_total : ( sizeof(TData) > fixedGrow ? 1 : ( fixedGrow / sizeof(TData) )));
            //const int tryTotal = 2*m_total;

            const int newTotal = ( tryTotal < newCount ) ? newCount : tryTotal;
            char *pNewData = new char[ newTotal * sizeof(TData) ];

            TData *dest = (TData *)pNewData;
            const TData *src = (const TData *)m_pData;

            for ( int i = 0; i < m_count; ++i )
            {
                new( dest ) TData;
                *dest++ = *src++;
            }

            TData *ptr = (TData *)m_pData;
            for ( int i = 0; i < m_count; ++i )
            {
                ptr->~TData();
                ++ptr;
            }

            if ( m_pData )
                delete[] m_pData;

            m_pData = pNewData;
            m_total = newTotal;
        }

        TData *ptr = (TData *)m_pData + m_count;
        for ( int i = m_count; i < newCount; ++i )
        {
            new( ptr ) TData;
            ++ptr;
        }

        m_count = newCount;
    }

    TData & PushFront()
    {
        Resize( m_count + 1 );

        TData *dest = (TData *)m_pData + (m_count-1);
        const TData *src = (const TData *)m_pData + (m_count-2);
        for ( int i = 0; i < m_count-1; ++i )
            *dest-- = *src--;

        TData *ptr = (TData *)m_pData;
        ptr->~TData();
        new( ptr ) TData;
        return *ptr;
    }

    void PushFront( const TData & value )
    {
        PushFront() = value;
    }

    TData & PushBack()
    {
        Resize( m_count + 1 );
        return ((TData *)m_pData)[ m_count-1 ];
    }

    void PushBack( const TData & value )
    {
        PushBack() = value;
    }

    void PopBack( TData *pOut=0 )
    {
        if ( m_count <= 0 )
        {
            if ( pOut )
                *pOut = TData();

            return;
        }

        if ( pOut )
            *pOut = ((const TData *)m_pData)[ m_count-1 ];

        Resize( m_count - 1 );
    }

    void RemoveByLast( int index )
    {
        if ( index < 0 || index >= m_count )
            return;

        TData *ptr = (TData *)m_pData;

        if ( index != m_count-1 )
            ptr[ index ] = ptr[ m_count-1 ];

        (&ptr[ m_count-1 ])->~TData();
        --m_count;
    }

    void Cleanup()
    {
        if ( m_pData )
        {
            Clear();
            delete[] m_pData;
            Reset();
        }
    }

private:
    void Reset()
    {
        m_pData = 0;
        m_count = 0;
        m_total = 0;
    }

    void Copy( const PWArray & from )
    {
        Clear();
        
        if ( from.m_count <= 0 )
            return;

        Resize( from.m_count );

        TData *dest = (TData *)m_pData;
        const TData *src = (const TData *)from.m_pData;
        for ( int i = 0; i < m_count; ++i )
        {
            *dest++ = *src++;
        }
    }

private:
    char *m_pData;
    int m_count;
    int m_total;
};

//----------------------------------------------------------------

#endif
