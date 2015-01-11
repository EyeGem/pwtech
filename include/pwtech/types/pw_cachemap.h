// pw_cachemap.h
#ifndef pw_cachemapH
#define pw_cachemapH

#include <memory.h>
#include <string.h>

#include "pw_ints.h"

//----------------------------------------------------------------

template<typename T, int MaxCount>
class PWCacheMap
{
private: // NO COPY
    PWCacheMap( const PWCacheMap & );
    void operator = ( const PWCacheMap & );

public:
    PWCacheMap()
    {
        Clear();
    }

    ~PWCacheMap()
    {
    }

public:
    T *Create( PWUInt32 key )
    {
        if ( !m_free )
            return 0;

        SBucket & bucket = m_buckets[ key % NumBuckets ];

        SItem *pItem = bucket.first;
        while ( pItem )
        {
            if ( pItem->key == key )
                return 0;

            pItem = pItem->next;
        }

        pItem = m_free;
        m_free = pItem->next;
        pItem->next = bucket.first;
        bucket.first = pItem;  

        pItem->key = key;
        return &pItem->data;
    }

    bool Remove( PWUInt32 key )
    {
        if ( key < 0 )
            return false;

        SBucket & bucket = m_buckets[ key % NumBuckets ];

        SItem *pPrev = 0;
        SItem *pItem = bucket.first;
        while ( pItem )
        {
            if ( pItem->key == key )
            {
                pItem->key = -1;
                pItem->data = T();

                if ( pPrev ) pPrev->next = pItem->next; else bucket.first = pItem->next;
                pItem->next = m_free;
                m_free = pItem;

                return true;
            }

            pPrev = pItem;
            pItem = pItem->next;
        }

        return false;
    }

public:
    const T *Read( PWUInt32 key ) const
    {
        if ( key < 0 )
            return 0;

        const SItem *pItem = m_buckets[ key % NumBuckets ].first;
        while ( pItem )
        {
            if ( pItem->key == key )
                return &pItem->data;

            pItem = pItem->next;
        }

        return 0;
    }

    T *Modify( PWUInt32 key )
    {
        if ( key < 0 )
            return 0;

        SItem *pItem = m_buckets[ key % NumBuckets ].first;
        while ( pItem )
        {
            if ( pItem->key == key )
                return &pItem->data;

            pItem = pItem->next;
        }

        return 0;
    }

public:
    void Clear()
    {
        memset( m_buckets, 0, sizeof(SBucket) * NumBuckets );
        //for ( int i = 0; i < NumBuckets; ++i )
        //    m_buckets[i].first = 0;

        for ( int i = 0; i < MaxCount; ++i )
        {
            m_items[i].key = 0;
            m_items[i].data = T();
            m_items[i].next = &m_items[i+1];
        }
        m_items[ MaxCount-1 ].next = 0;
        m_free = &m_items[0];
    }

private:
    struct SItem { PWUInt32 key; T data; SItem *next; };
    SItem m_items[ MaxCount ];
    SItem *m_free;

    static const int NumBuckets = MaxCount * 2;
    struct SBucket { SItem *first; };
    SBucket m_buckets[ NumBuckets ];
};

//----------------------------------------------------------------

#endif
