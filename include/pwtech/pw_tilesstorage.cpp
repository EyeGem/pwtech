// pw_tilesstorage.cpp
#include "pw_tilesstorage.h"

#include "pw_lookups.h"

#include <memory.h>
#include <string.h>

#include "types/pw_buffer.h"

//----------------------------------------------------------------

struct SPWTilesStorageData
{
    IPWFile      *m_pFile;
    PWUInt8       m_itemDataSize;
    bool          m_readOnly;

    PWUInt32      m_firstFreePageIndex;
    PWUInt32      m_endPageIndex;

    PWUInt32      m_tilesPages[ 65536 ];
    PWUInt32     *m_tilesDataPages[ 65536 ];
    PWUInt32      m_tilesDataPagesCount[ 65536 ];
    PWUInt32      m_tilesTotalDataSizes[ 65536 ];
    EPWTileStatus m_tilesDataStatus[ 65536 ];

    PWArray<PWUInt32> m_tmpIndexes;

    SPWTilesStorageData()
    {
        for ( int tile = 0; tile < 65536; ++tile )
            m_tilesDataPages[ tile ] = 0;

        Reset();
    }

    void Reset()
    {
        m_pFile = 0;
        m_itemDataSize = 0;
        m_readOnly = true;

        m_firstFreePageIndex = 0;
        m_endPageIndex = 0;

        for ( int tile = 0; tile < 65536; ++tile )
        {
            if ( m_tilesDataPages[ tile ] )
                delete[] m_tilesDataPages[ tile ];

            m_tilesPages[ tile ] = 0;
            m_tilesDataPages[ tile ] = 0;
            m_tilesDataPagesCount[ tile ] = 0;
            m_tilesTotalDataSizes[ tile ] = 0;
            m_tilesDataStatus[ tile ] = PWTILE_NOTINIT; // default
        }
    }

    bool CloseTiles()
    {
        if ( !m_pFile )
            return false;

        m_pFile->Close();
        Reset();

        return true;
    }

    bool NewTiles( IPWFile *pFile, PWUInt8 itemDataSize )
    {
        CloseTiles();

        if ( !pFile )
            return false;
        
        if ( !pFile->CanWrite() )
            return false;
            
        if ( itemDataSize <= 0 )
            return false;

        if ( !pFile->Seek(0) )
            return false;

        if ( !pFile->Write( "PWTILES_", 9 ) ) // write not full signature while creating file
            return false;

        if ( !pFile->Write( &itemDataSize, 1 ) )
            return false;

        char zerobuf[4096];
        memset( zerobuf, 0, sizeof(zerobuf) );

        if ( !pFile->Write( &zerobuf, 6 ) )
            return false;

        const PWUInt32 firstFreePageIndex = 0; // 0=no free
        if ( !pFile->Write( &firstFreePageIndex, 4 ) )
            return false;

        const PWUInt32 endPageIndex = 65; // 0=header 1..64=table 65=last
        if ( !pFile->Write( &endPageIndex, 4 ) )
            return false;

        if ( !pFile->Write( &zerobuf, 4096-24 ) ) // rest of header page
            return false;

        for ( int i = 0; i < 64; ++i )
        {
            if ( !pFile->Write( &zerobuf, 4096 ) ) // 0=no data
                return false;
        }    

        if ( !pFile->Seek(0) )
            return false;

        if ( !pFile->Write( "PWTILES1", 9 ) ) // write full signature
            return false;

        m_pFile = pFile;
        m_itemDataSize = itemDataSize;
        m_readOnly = false;
        //
        m_firstFreePageIndex = firstFreePageIndex;
        m_endPageIndex = endPageIndex;

        return true;
    }

    bool OpenTiles( IPWFile *pFile, PWUInt8 itemDataSize, bool readOnly, bool anyItemDataSize = false )
    {
        CloseTiles();

        if ( !pFile )
            return false;

        if ( !pFile->Seek(0) )
            return false;

        char buf[4096];

        if ( !pFile->Read( buf, 4096 ) ) // read header
            return false;

        if ( buf[0] != 'P' ) return false;
        if ( buf[1] != 'W' ) return false;
        if ( buf[2] != 'T' ) return false;
        if ( buf[3] != 'I' ) return false;
        if ( buf[4] != 'L' ) return false;
        if ( buf[5] != 'E' ) return false;
        if ( buf[6] != 'S' ) return false;
        if ( buf[7] != '1' ) return false;
        if ( buf[8] !=  0  ) return false;

        PWUInt8 gotItemDataSize = 0;
        memcpy( &gotItemDataSize, buf+9, 1 );
        if ( gotItemDataSize <= 0 || ( !anyItemDataSize && itemDataSize != gotItemDataSize ) )
            return false;

        PWUInt32 firstFreePageIndex = 0;
        memcpy( &firstFreePageIndex, buf+16, 4 );
        if ( firstFreePageIndex > 0 && firstFreePageIndex < 65 )
            return false; // corrupted

        PWUInt32 endPageIndex = 0;
        memcpy( &endPageIndex, buf+20, 4 );
        if ( endPageIndex < 65 )
            return false; // corrupted

        if ( !pFile->Read( m_tilesPages, 65536 * 4 ) )
            return false;

        m_pFile = pFile;
        m_itemDataSize = gotItemDataSize;
        m_readOnly = !pFile->CanWrite();
        //
        m_firstFreePageIndex = firstFreePageIndex;
        m_endPageIndex = endPageIndex;

        return true;
    }

    bool InitTile( int tile )
    {
        if ( !m_pFile )
            return false;

        if ( tile < 0 || tile >= 65536 )
            return false;
        
        if ( m_tilesDataPages[ tile ] ) // already init?
            return false;

        m_tilesDataPages[ tile ] = new PWUInt32[1];
        m_tilesDataPagesCount[ tile ] = 0;
        m_tilesTotalDataSizes[ tile ] = 0;
        m_tilesDataStatus[ tile ] = PWTILE_NOTINIT;

        PWUInt32 tilePage = m_tilesPages[ tile ];
        if ( !tilePage )
        {
            m_tilesDataStatus[ tile ] = PWTILE_EMPTY;
            return true;
        }

        PWFileOffs fileOffs;
        fileOffs = tilePage;
        fileOffs <<= 12; // by 4K

        if ( !m_pFile->Seek( fileOffs ) )
            return false;

        PWUInt32 nextPageIndex;
        if ( !m_pFile->Read( &nextPageIndex, 4 ) )
            return false;

        PWUInt32 lastPageIndex;
        if ( !m_pFile->Read( &lastPageIndex, 4 ) )
            return false;

        PWUInt32 flags;
        if ( !m_pFile->Read( &flags, 4 ) )
            return false;

        PWUInt32 dataPagesCount;
        if ( !m_pFile->Read( &dataPagesCount, 4 ) )
            return false;

        PWUInt32 dataTotalSize;
        if ( !m_pFile->Read( &dataTotalSize, 4 ) )
            return false;

        const PWUInt32 maxDataPagesCount = 1024*1024; // sanity limit (1Gb)
        if ( dataPagesCount == 0 || dataPagesCount > maxDataPagesCount )
        {
            m_tilesDataStatus[ tile ] = PWTILE_EMPTY;
            return true;
        }

        if ( dataTotalSize > (dataPagesCount * 4096) )
        {
            m_tilesDataStatus[ tile ] = PWTILE_ERROR;
            return false;
        }

        m_tmpIndexes.Resize( dataPagesCount );
        PWUInt32 pageIndex = 0;
        PWUInt32 pagesLeft = dataPagesCount;

        PWUInt32 pagesToRead = pagesLeft;
        if ( pagesToRead > 1019 )
            pagesToRead = 1019;

        if ( !m_pFile->Read( &m_tmpIndexes[ pageIndex ], pagesToRead*4 ) )
            return false;

        pagesLeft -= pagesToRead;
        pageIndex += pagesToRead;

        while ( pagesLeft > 0 )
        {
            if ( nextPageIndex < 65 )
                return false;

            fileOffs = nextPageIndex;
            fileOffs <<= 12; // by 4K

            if ( !m_pFile->Seek( fileOffs ) )
                return false;

            if ( !m_pFile->Read( &nextPageIndex, 4 ) )
                return false;

            pagesToRead = pagesLeft;
            if ( pagesToRead > 1023 )
                pagesToRead = 1023;

            if ( !m_pFile->Read( &m_tmpIndexes[ pageIndex ], pagesToRead*4 ) )
                return false;

            pagesLeft -= pagesToRead;
            pageIndex += pagesToRead;
        }

        for ( PWUInt32 idx = 0; idx < dataPagesCount; ++idx )
            if ( m_tmpIndexes[ idx ] < 65 )
                return false;

        delete[] m_tilesDataPages[ tile ];
        m_tilesDataPages[ tile ] = new PWUInt32[ dataPagesCount ];
        memcpy( m_tilesDataPages[ tile ], &m_tmpIndexes[0], dataPagesCount*4 );

        m_tilesDataPagesCount[ tile ] = dataPagesCount;
        m_tilesTotalDataSizes[ tile ] = dataTotalSize;
        m_tilesDataStatus[ tile ] = (( flags & 0xFF ) == 1) ? PWTILE_BAKED : PWTILE_NOT_BAKED;

        return true;
    }

    PWUInt32 AllocPage()
    {
        if ( !m_pFile || m_readOnly )
            return 0;

        if ( !m_firstFreePageIndex )
        {
            if ( m_endPageIndex < 65 )
                return 0; // corrupted

            PWFileOffs fileOffs;
            fileOffs = m_endPageIndex;
            fileOffs <<= 12; // by 4K

            if ( !m_pFile->Seek( fileOffs ) )
                return 0;

            char zerobuf[4096];
            memset( zerobuf, 0, sizeof(zerobuf) );

            if ( !m_pFile->Write( zerobuf, 4096 ) )
                return 0;

            m_endPageIndex += 1;
            if ( !m_pFile->Seek( 20 ) ) return 0;
            if ( !m_pFile->Write( &m_endPageIndex, 4 ) ) return 0;

            m_firstFreePageIndex = m_endPageIndex - 1;
            if ( !m_pFile->Seek( 16 ) ) return 0;
            if ( !m_pFile->Write( &m_firstFreePageIndex, 4 ) ) return 0;
        }

        PWFileOffs fileOffs;
        fileOffs = m_firstFreePageIndex;
        fileOffs <<= 12; // by 4K

        if ( !m_pFile->Seek( fileOffs ) )
            return 0;

        PWUInt32 nextFreePageIndex = 0;
        if ( !m_pFile->Read( &nextFreePageIndex, 4 ) )
            return 0;

        // zero page data
        {
            if ( !m_pFile->Seek( fileOffs ) )
                return 0;

            char zerobuf[4096];
            memset( zerobuf, 0, sizeof(zerobuf) );

            if ( !m_pFile->Write( zerobuf, 4096 ) )
                return 0;
        }

        if ( !m_pFile->Seek( 16 ) )
            return 0;
                        
        if ( !m_pFile->Write( &nextFreePageIndex, 4 ) )
            return 0;

        const PWUInt32 allocatedPageIndex = m_firstFreePageIndex;
        m_firstFreePageIndex = nextFreePageIndex;

        return allocatedPageIndex;
    }

    bool MarkTileBaked( int tile )
    {
        if ( !m_pFile || m_readOnly )
            return false;

        if ( tile < 0 || tile >= 65536 )
            return false;

        if ( m_tilesDataPages[ tile ] ) // purge from cache
        {
            delete[] m_tilesDataPages[ tile ];
            m_tilesDataPages[ tile ] = 0;
            m_tilesDataPagesCount[ tile ] = 0;
            m_tilesDataStatus[ tile ] = PWTILE_NOTINIT;
        }

        if ( !m_tilesPages[ tile ] )
            return false;

        if ( m_tilesPages[ tile ] < 65 )
            return false; // corrupted

        PWFileOffs fileOffs;
        fileOffs = m_tilesPages[ tile ];
        fileOffs <<= 12; // by 4K
        fileOffs += 8; // offset flags

        if ( !m_pFile->Seek( fileOffs ) )
            return false;

        PWUInt32 val_flags = 0;
        if ( !m_pFile->Read( &val_flags, 4 ) )
            return false;

        val_flags |= 1; // BAKED

        if ( !m_pFile->Seek( fileOffs ) )
            return false;

        if ( !m_pFile->Write( &val_flags, 4 ) )
            return false;

        return true;
    }

    bool VerifyWriteOffset( int tile, PWUInt32 offset )
    {
        if ( !m_pFile || m_readOnly )
            return false;

        if ( tile < 0 || tile >= 65536 )
            return false;

        if ( !m_tilesPages[ tile ] )
            return ( offset == 0 );

        if ( m_tilesPages[ tile ] < 65 )
            return false; // corrupted

        PWFileOffs fileOffs;
        fileOffs = m_tilesPages[ tile ];
        fileOffs <<= 12; // by 4K

        if ( !m_pFile->Seek( fileOffs ) )
            return false;

        PWUInt32 val_nextPageIndex = 0;
        PWUInt32 val_lastPageIndex = 0;
        PWUInt32 val_flags = 0;
        PWUInt32 val_dataPagesCount = 0;
        PWUInt32 val_dataTotalSize = 0;

        if ( !m_pFile->Read( &val_nextPageIndex, 4 ) ) return false;
        if ( !m_pFile->Read( &val_lastPageIndex, 4 ) ) return false;
        if ( !m_pFile->Read( &val_flags, 4 ) ) return false;
        if ( !m_pFile->Read( &val_dataPagesCount, 4 ) ) return false;
        if ( !m_pFile->Read( &val_dataTotalSize, 4 ) ) return false;

        return val_dataTotalSize == offset;
    }

    bool WriteTileData( int tile, const void *src, PWUInt32 size )
    {
        if ( !m_pFile || m_readOnly )
            return false;

        if ( !src || size == 0 )
            return false;

        if ( tile < 0 || tile >= 65536 )
            return false;
        
        if ( m_tilesDataPages[ tile ] ) // purge from cache
        {
            delete[] m_tilesDataPages[ tile ];
            m_tilesDataPages[ tile ] = 0;
            m_tilesDataPagesCount[ tile ] = 0;
            m_tilesDataStatus[ tile ] = PWTILE_NOTINIT;
        }

        if ( !m_tilesPages[ tile ] )
        {
            const PWUInt32 firstPageIndex = AllocPage();
            if ( firstPageIndex < 65 )
                return false;

            PWFileOffs fileOffs;
            fileOffs = firstPageIndex;
            fileOffs <<= 12; // by 4K

            if ( !m_pFile->Seek( fileOffs ) )
                return false;

            PWUInt32 wr_nextPageIndex = 0;
            PWUInt32 wr_lastPageIndex = firstPageIndex;
            PWUInt32 wr_flags = 0;
            PWUInt32 wr_dataPagesCount = 0;
            PWUInt32 wr_dataTotalSize = 0;
            
            char wr_zeroes[4096];
            memset( wr_zeroes, 0, sizeof(wr_zeroes) );

            if ( !m_pFile->Write( &wr_nextPageIndex, 4 ) ) return false;
            if ( !m_pFile->Write( &wr_lastPageIndex, 4 ) ) return false;
            if ( !m_pFile->Write( &wr_flags, 4 ) ) return false;
            if ( !m_pFile->Write( &wr_dataPagesCount, 4 ) ) return false;
            if ( !m_pFile->Write( &wr_dataTotalSize, 4 ) ) return false;
            if ( !m_pFile->Write( wr_zeroes, 1019*4 ) ) return false;

            if ( !m_pFile->Seek( 4096 + tile * 4 ) )
                return false;

            if ( !m_pFile->Write( &firstPageIndex, 4 ) )
                return false;

            m_tilesPages[ tile ] = firstPageIndex;
        }

        if ( m_tilesPages[ tile ] < 65 )
            return false; // corrupted

        PWFileOffs fileOffs;
        fileOffs = m_tilesPages[ tile ];
        fileOffs <<= 12; // by 4K

        if ( !m_pFile->Seek( fileOffs ) )
            return false;

        PWUInt32 val_nextPageIndex = 0;
        PWUInt32 val_lastPageIndex = 0;
        PWUInt32 val_flags = 0;
        PWUInt32 val_dataPagesCount = 0;
        PWUInt32 val_dataTotalSize = 0;

        if ( !m_pFile->Read( &val_nextPageIndex, 4 ) ) return false;
        if ( !m_pFile->Read( &val_lastPageIndex, 4 ) ) return false;
        if ( !m_pFile->Read( &val_flags, 4 ) ) return false;
        if ( !m_pFile->Read( &val_dataPagesCount, 4 ) ) return false;
        if ( !m_pFile->Read( &val_dataTotalSize, 4 ) ) return false;

        if ( val_lastPageIndex < 65 ) return false;

        PWUInt32 tablePageIndex = val_lastPageIndex;
        PWUInt32 tableItemIndex = val_dataTotalSize >> 12; // by 4K
        PWUInt32 dataPageOffset = val_dataTotalSize & 4095;

        if ( tableItemIndex < 1019 && val_lastPageIndex != m_tilesPages[ tile ] )
            return false;

        if ( tableItemIndex < 1019 )
            tableItemIndex = 5 + tableItemIndex;
        else
            tableItemIndex = 1 + ((tableItemIndex-1019) % 1023);

        while ( size > 0 )
        {
            PWUInt32 sizeWrite = 4096 - dataPageOffset;
            if ( size < sizeWrite )
                sizeWrite = size;

            fileOffs = tablePageIndex;
            fileOffs <<= 12; // by 4K
            fileOffs += tableItemIndex * 4;
           
            if ( !m_pFile->Seek( fileOffs ) )
                return false;

            PWUInt32 dataPageIndex = 0;
            if ( !m_pFile->Read( &dataPageIndex, 4 ) )
                return false;

            if ( !dataPageIndex )
            {
                val_dataPagesCount += 1;

                dataPageIndex = AllocPage();
                if ( !dataPageIndex )
                    return false;

                if ( !m_pFile->Seek( fileOffs ) )
                    return false;

                if ( !m_pFile->Write( &dataPageIndex, 4 ) )
                    return false;
            }

            fileOffs = dataPageIndex;
            fileOffs <<= 12; // by 4K
            fileOffs += dataPageOffset;
            
            if ( !m_pFile->Seek( fileOffs ) )
                return false;

            if ( !m_pFile->Write( src, sizeWrite ) )
                return false;

            src = (const char *)src + sizeWrite;
            size -= sizeWrite;
            val_dataTotalSize += sizeWrite;

            dataPageOffset += sizeWrite;
            if ( dataPageOffset >= 4096 )
            {
                dataPageOffset = 0;

                if ( tableItemIndex < 1023 )
                {
                    tableItemIndex += 1;
                }
                else
                {
                    const PWUInt32 newTablePageIndex = AllocPage();
                    if ( !newTablePageIndex )
                        return false;

                    fileOffs = tablePageIndex;
                    fileOffs <<= 12; // by 4K
                    
                    if ( !m_pFile->Seek( fileOffs ) )
                        return false;

                    if ( !m_pFile->Write( &newTablePageIndex, 4 ) )
                        return false;

                    if ( tablePageIndex == m_tilesPages[ tile ] )
                        val_nextPageIndex = newTablePageIndex;

                    tablePageIndex = newTablePageIndex;
                    tableItemIndex = 1;
                }
            }
        }

        val_lastPageIndex = tablePageIndex;
        
        fileOffs = m_tilesPages[ tile ];
        fileOffs <<= 12; // by 4K

        if ( !m_pFile->Seek( fileOffs ) )
            return false;

        if ( !m_pFile->Write( &val_nextPageIndex, 4 ) ) return false;
        if ( !m_pFile->Write( &val_lastPageIndex, 4 ) ) return false;
        if ( !m_pFile->Write( &val_flags, 4 ) ) return false;
        if ( !m_pFile->Write( &val_dataPagesCount, 4 ) ) return false;
        if ( !m_pFile->Write( &val_dataTotalSize, 4 ) ) return false;

        return true;
    }

    bool ReadTileData( int tile, PWUInt32 offset, void *dest, PWUInt32 size )
    {
        if ( !m_pFile )
            return false;
        
        if ( !dest || size == 0 )
            return false;

        if ( tile < 0 || tile >= 65536 )
            return false;

        if ( !m_tilesDataPages[ tile ] )
        {
            if ( !InitTile( tile ) )
                return false;
        }

        if ( offset + size > m_tilesTotalDataSizes[ tile ] )
        {
            if ( offset > m_tilesTotalDataSizes[ tile ] )
                return false;

            size = m_tilesTotalDataSizes[ tile ] - offset;
        }

        PWUInt32 dataPageIndex = (offset >> 12);
        PWUInt32 dataPageOffset = (offset & 4095);

        const PWUInt32 *tileDataPages = m_tilesDataPages[ tile ];
        const PWUInt32 dataPagesCount = m_tilesDataPagesCount[ tile ];
        PWUInt8 *wr = (PWUInt8 *)dest;

        PWFileOffs fileOffs;

        while ( size > 0 )
        {
            if ( dataPageIndex >= dataPagesCount )
                return false;

            PWUInt32 sizeRead = 4096 - dataPageOffset;
            if ( size < sizeRead )
                sizeRead = size;

            const PWUInt32 dataPageNum = tileDataPages[ dataPageIndex ];

            fileOffs = dataPageNum;
            fileOffs <<= 12; // by 4K
            fileOffs += dataPageOffset;
           
            if ( !m_pFile->Seek( fileOffs ) )
                return false;

            if ( !m_pFile->Read( wr, sizeRead ) )
                return false;

            wr += sizeRead;
            size -= sizeRead;

            dataPageIndex += 1;
            dataPageOffset = 0;
        }

        return true;
    }

private:
    TPWTileMixFunc          m_bake_mix_func;

    const SPWTilePointDesc *m_bake_descs;
    const char             *m_bake_datas;
    PWArray<int>            m_bake_indices;

    PWArray<char>           m_bake_mixdata;
    PWArray<char>           m_bake_tmpbuf;

public:
    PWArray<PWBuffer>           m_bake_sbuf; // subdivs buffers
    PWArray<PWBuffer>           m_bake_dbuf; // data buffers

    int                         m_bake_sum_chunk_shift;
    PWArray<PWArray<PWUInt32> > m_bake_sums;

private:
    bool BakeTileNodeRecursive( int level, PWUInt32 idx_first, PWUInt32 idx_count, float px, float py, float pz, float step )
    {
        if ( level < m_bake_sbuf.GetCount()-1 )
        {
            int counts[8];
            for ( int i = 0; i < 8; ++i )
                counts[i] = 0;

            for ( PWUInt32 idx = 0; idx < idx_count; ++idx )
            {
                const SPWTilePointDesc & desc = m_bake_descs[ m_bake_indices[ idx_first + idx ] ];

                int octidx = 0;
                octidx |= ( ( desc.x >= px ) ? 1 : 0 );
                octidx |= ( ( desc.y >= py ) ? 2 : 0 );
                octidx |= ( ( desc.z >= pz ) ? 4 : 0 );

                counts[ octidx ] += 1;
            }

            PWUInt8 oct = 0;
            for ( int i = 0; i < 8; ++i )
                if ( counts[i] > 0 )
                    oct |= (1<<i);

            m_bake_sbuf[ level ].Write( &oct, 1 );

            int starts[8];
            int offsets[8];

            for ( int i = 0; i < 8; ++i )
            {
                starts[i] = ( i <= 0 ) ? 0 : ( starts[i-1] + counts[i-1] );
                offsets[i] = 0;
            }

            PWUInt32 done_count = 0;
            int cur_div = 0;
            while ( cur_div < 8 && done_count < idx_count )
            {
                if ( offsets[ cur_div ] >= counts[ cur_div ] )
                {
                    ++cur_div;
                    continue;
                }

                const PWUInt32 at_idx = idx_first + starts[ cur_div ] + offsets[ cur_div ];

                const SPWTilePointDesc & desc = m_bake_descs[ m_bake_indices[ at_idx ] ];

                int octidx = 0;
                octidx |= ( ( desc.x >= px ) ? 1 : 0 );
                octidx |= ( ( desc.y >= py ) ? 2 : 0 );
                octidx |= ( ( desc.z >= pz ) ? 4 : 0 );

                if ( octidx != cur_div )
                {
                    const PWUInt32 to_idx = idx_first + starts[ octidx ] + offsets[ octidx ];

                    const int tmp_idx = m_bake_indices[ at_idx ];
                    m_bake_indices[ at_idx ] = m_bake_indices[ to_idx ];
                    m_bake_indices[ to_idx ] = tmp_idx;

                    offsets[ octidx ] += 1;
                    ++done_count;
                }
                else
                {
                    offsets[ cur_div ] += 1;
                    ++done_count;
                }
            }

            const float step2 = step * 0.5f;

            int avg_count = 0;
            for ( int i = 0; i < 8; ++i )
            {
                if ( counts[i] > 0 )
                {
                    const float cpx = px + step * ((i&1)?1:-1);
                    const float cpy = py + step * ((i&2)?1:-1);
                    const float cpz = pz + step * ((i&4)?1:-1);

                    if ( !BakeTileNodeRecursive( level+1, idx_first + starts[i], counts[i], cpx, cpy, cpz, step2 ) )
                        return false;

                    ++avg_count;
                }
            }

            int blk = m_bake_dbuf[ level+1 ].GetBlocksCount()-1;
            if ( blk < 0 )
                return false;

            PWUInt32 block_left;
            const char *ptr = m_bake_dbuf[ level+1 ].GetBlockData( blk, block_left );
            if ( !ptr || block_left == 0 )
                return false;
            
            const PWUInt32 mixsize = avg_count * m_itemDataSize;
            m_bake_mixdata.Resize( mixsize );

            PWUInt32 left = mixsize;
            while ( left > 0 )
            {
                if ( block_left == 0 )
                {
                    blk -= 1;
                    if ( blk < 0 )
                        return false;

                    ptr = m_bake_dbuf[ level+1 ].GetBlockData( blk, block_left );
                    if ( !ptr || block_left == 0 )
                        return false;
                }

                PWUInt32 len = left;
                if ( len > block_left )
                    len = block_left;

                left -= len;
                block_left -= len;
                memcpy( &m_bake_mixdata[left], ptr + block_left, len );
            }

            m_bake_tmpbuf.Resize( m_itemDataSize );
            m_bake_mix_func( &m_bake_tmpbuf[0], &m_bake_mixdata[0], avg_count, level );

            m_bake_dbuf[ level ].Write( &m_bake_tmpbuf[0], m_itemDataSize );
        }
        else
        {
            //PWUInt8 oct = 0; // leaf
            //m_bake_sbuf[ level ].Write( &oct, 1 );

            PWUInt32 mixcount = idx_count;
            //if ( mixcount > 65536 ) // sanity limit (per atom)
            //    mixcount = 65536;

            const PWUInt32 mixsize = mixcount * m_itemDataSize;
            m_bake_mixdata.Resize( mixsize );

            for ( PWUInt32 idx = 0; idx < mixcount; ++idx )
            {
                void *dest = &m_bake_mixdata[0] + m_itemDataSize * idx;
                const char *src = m_bake_datas + m_itemDataSize * m_bake_indices[ idx_first + idx ];
                memcpy( dest, src, m_itemDataSize );
            }

            m_bake_tmpbuf.Resize( m_itemDataSize );
            m_bake_mix_func( &m_bake_tmpbuf[0], &m_bake_mixdata[0], idx_count, level );

            m_bake_dbuf[ level ].Write( &m_bake_tmpbuf[0], m_itemDataSize );
        }

        return true;
    }

    void BakeTileCalcSums()
    {
        PWLUT_ByteCounts lut_byte_counts;

        for ( int level = 0; level < m_bake_sbuf.GetCount(); ++level )
        {
            m_bake_sums[ level ].Clear();

            if ( m_bake_sum_chunk_shift < 0 )
                continue;

            PWUInt32 sz = 0;
            PWUInt32 block_sz;

            const int numBlocks = m_bake_sbuf[ level ].GetBlocksCount();
            for ( int i = 0; i < numBlocks; ++i )
            {
                m_bake_sbuf[ level ].GetBlockData( i, block_sz );
                sz += block_sz;
            }

            if ( sz == 0 )
                continue;

            const int num_chunks = (( sz - 1 ) >> m_bake_sum_chunk_shift ) + 1;
            m_bake_sums[ level ].Resize( num_chunks, 0 );

            int block_idx = 0;
            const PWUInt8 *ptr = (const PWUInt8 *)m_bake_sbuf[ level ].GetBlockData( block_idx, block_sz );
            const PWUInt32 chunk_full_size = ( 1 << m_bake_sum_chunk_shift );
            const PWUInt32 chunk_last_size = ( (sz % chunk_full_size) == 0 ) ? chunk_full_size : ( sz % chunk_full_size );
            PWUInt32 chunk_sz = 0;
            int chunk_sum = 0;
            int chunk_idx = 0;
            int total_sum = 0;
            //
            while ( sz > 0 && ptr )
            {
                const PWUInt32 target_sz = (( chunk_idx == num_chunks-1 ) ? chunk_last_size : chunk_full_size );
                PWUInt32 len = target_sz - chunk_sz;
                if ( len > block_sz )
                    len = block_sz;

                for ( PWUInt32 idx = 0; idx < len; ++idx )
                    chunk_sum += lut_byte_counts.lookup[ ptr[idx] ];

                sz -= len;
                ptr += len;
                chunk_sz += len;
                block_sz -= len;

                if ( chunk_sz >= target_sz )
                {
                    m_bake_sums[ level ][ chunk_idx ] = total_sum;
                    total_sum += chunk_sum;
                    chunk_idx += 1;
                    chunk_sum = 0;
                    chunk_sz = 0;
                }

                if ( block_sz == 0 )
                {
                    block_idx += 1;
                    ptr = (const PWUInt8 *)m_bake_sbuf[ level ].GetBlockData( block_idx, block_sz );
                }
            }
        }
    }

public:
    bool BakeTile( int num_levels, PWUInt32 numPoints, const SPWTilePointDesc *descs, const void *datas, TPWTileMixFunc mix_func )
    {
        // NO ARGS CHECKS!

        m_bake_mix_func = mix_func;

        m_bake_descs = descs;
        m_bake_datas = (const char *)datas;

        m_bake_indices.Resize( numPoints );

        for ( PWUInt32 ptIdx = 0; ptIdx < numPoints; ++ptIdx )
            m_bake_indices[ ptIdx ] = ptIdx;

        m_bake_sbuf.Resize( num_levels );
        m_bake_dbuf.Resize( num_levels );

        m_bake_sums.Clear();
        m_bake_sums.Resize( num_levels );
        m_bake_sum_chunk_shift = 8; // 256 bytes

        if ( numPoints <= 0 )
        {
            char zero = 0;
            m_bake_sbuf[0].Write( &zero, 1 );
            m_bake_mixdata.Resize( m_itemDataSize );
            memset( &m_bake_mixdata[0], 0, m_itemDataSize );
            m_bake_dbuf[0].Write( &m_bake_mixdata[0], m_itemDataSize );
            m_bake_sum_chunk_shift = 1;
            m_bake_sums[0].PushBack(0);
            return true;
        }

        if ( !BakeTileNodeRecursive( 0, 0, numPoints, 0.5f, 0.5f, 0.5f, 0.25f ) )
            return false;

        BakeTileCalcSums();
        return true;
    }

    void BakeTileCleanups()
    {
        m_bake_mix_func = 0;
        m_bake_descs = 0;
        m_bake_datas = 0;
        m_bake_indices.Cleanup();
        m_bake_mixdata.Cleanup();
        m_bake_sbuf.Cleanup();
        m_bake_dbuf.Cleanup();
        m_bake_sum_chunk_shift = 0;
        m_bake_sums.Cleanup();
    }
};

//----------------------------------------------------------------
// PW Tiles file format:
//
//   offset   size          content
//   -------------------------------------------------------------
//   0        8             "PWTILES1" format signature
//   8        1             0
//   9        1             BYTE itemDataSize
//   10       6             0
//   16       4             DWORD firstFreePageIndex (0=no free)
//   20       4             DWORD lastPageIndex      (page=4K)
//   ...      ...           0
//   4096     256K(64pages) DWORD pageIndex[ 65536 ] (0=empty)
//
//   First tile page:
//     DWORD nextPageIndex
//     DWORD lastPageIndex (to quickly append data)
//     DWORD flags (byte 1 = format: 0=points 1=baked)
//     DWORD dataPagesCount (should match)
//     DWORD dataTotalSize (should be less that dataPagesCount * 4K)
//     DWORD dataPageIndex[1019] (must be valid while ~ dataPagesCount)
//
//   Other tile pages:
//     DWORD nextPageIndex
//     DWORD dataPageIndex[1023] (must be valid while ~ dataPagesCount)
//
//   Free page:
//     DWORD nextFreePageIndex
//
//----------------------------------------------------------------

PWTilesStorage::PWTilesStorage()
{
    m_ptr = new SPWTilesStorageData();
}

PWTilesStorage::~PWTilesStorage()
{
    delete (SPWTilesStorageData *)m_ptr;
    m_ptr = 0;
}

//----------------------------------------------------------------

bool PWTilesStorage::NewTiles( IPWFile *pFile, PWUInt8 itemDataSize )
{
    SPWTilesStorageData & m = *(SPWTilesStorageData *)m_ptr;
    return m.NewTiles( pFile, itemDataSize );
}

bool PWTilesStorage::OpenTiles( IPWFile *pFile, PWUInt8 itemDataSize, bool readOnly )
{
    SPWTilesStorageData & m = *(SPWTilesStorageData *)m_ptr;
    return m.OpenTiles( pFile, itemDataSize, readOnly );
}

bool PWTilesStorage::CloseTiles()
{
    SPWTilesStorageData & m = *(SPWTilesStorageData *)m_ptr;
    return m.CloseTiles();
}

//----------------------------------------------------------------

bool PWTilesStorage::ReadBakedTile( int tile, PWUInt32 offset, void *dest, PWUInt32 size )
{
    //if ( !dest || size == 0 )
    //    return false;

    if ( tile < 0 || tile >= 65536 )
        return false;
    
    SPWTilesStorageData & m = *(SPWTilesStorageData *)m_ptr;
    //if ( !m.m_pFile )
    //    return false;

    //if ( !m.m_tilesDataPages[ tile ] )
    //{
    //    if ( !m.InitTile( tile ) )
    //        return false;
    //}

    EPWTileStatus status = m.m_tilesDataStatus[ tile ];
    if ( status == PWTILE_NOTINIT )
    {
        status = GetTileStatus( tile );
    }

    if ( m.m_tilesDataStatus[ tile ] != PWTILE_BAKED )
        return false;

    return m.ReadTileData( tile, offset, dest, size );
}

//----------------------------------------------------------------

bool PWTilesStorage::ReadBakedTileRootItemData( int tile, void *dest, PWUInt8 itemDataSize )
{
    if ( tile < 0 || tile >= 65536 )
        return false;
    
    SPWTilesStorageData & m = *(SPWTilesStorageData *)m_ptr;

    if ( !dest || itemDataSize != m.m_itemDataSize )
        return false;

    if ( GetTileStatus( tile  ) != PWTILE_BAKED )
        return false;

    PWUInt32 off_data;
    if ( !m.ReadTileData( tile, 24, &off_data, 4 ) )
        return false;
    
    if ( !m.ReadTileData( tile, off_data + 4, dest, itemDataSize ) )
        return false;

    return true;
}

//----------------------------------------------------------------

EPWTileStatus PWTilesStorage::GetTileStatus( int tile )
{
    if ( tile < 0 || tile >= 65536 )
        return PWTILE_ERROR;

    SPWTilesStorageData & m = *(SPWTilesStorageData *)m_ptr;
    if ( !m.m_pFile )
        return PWTILE_ERROR;

    if ( !m.m_tilesDataPages[ tile ] )
    {
        if ( !m.InitTile( tile ) )
            return m.m_tilesDataStatus[ tile ];
    }

    return m.m_tilesDataStatus[ tile ];
}

//----------------------------------------------------------------

bool PWTilesStorage::ClearTile( int tile )
{
    if ( tile < 0 || tile >= 65536 )
        return false;

    SPWTilesStorageData & m = *(SPWTilesStorageData *)m_ptr;
    if ( !m.m_pFile || m.m_readOnly )
        return false;

    if ( m.m_tilesDataPages[ tile ] )
    {
        delete[] m.m_tilesDataPages[ tile ];
        m.m_tilesDataPages[ tile ] = 0;
        m.m_tilesDataPagesCount[ tile ] = 0;
        m.m_tilesDataStatus[ tile ] = PWTILE_NOTINIT;
    }

    PWUInt32 pageIndex = m.m_tilesPages[ tile ];
    if ( !pageIndex )
        return true;

    if ( !m.m_pFile->Seek( 4096 + tile * 4 ) )
        return false; // failure

    PWUInt32 zeroIndex = 0;
    if ( !m.m_pFile->Write( &zeroIndex, 4 ) )
        return false; // failure

    m.m_tilesPages[ tile ] = 0;

    while ( pageIndex )
    {
        if ( pageIndex < 65 )
            return false; // corrupted

        PWFileOffs fileOffs;
        fileOffs = pageIndex;
        fileOffs <<= 12; // by 4K

        if ( !m.m_pFile->Seek( fileOffs ) )
            return false; // failure

        if ( !m.m_pFile->Read( &pageIndex, 4 ) )
            return false; // failure

        if ( !m.m_pFile->Seek( fileOffs ) )
            return false; // failure

        if ( !m.m_pFile->Write( &m.m_firstFreePageIndex, 4 ) )
            return false; // failure

        m.m_firstFreePageIndex = pageIndex;
    }

    if ( !m.m_pFile->Seek( 16 ) )
        return false; // failure

    if ( !m.m_pFile->Write( &m.m_firstFreePageIndex, 4 ) )
        return false; // failure

    return true;
}

//----------------------------------------------------------------

bool PWTilesStorage::WriteTilePoints( int tile, int count, const SPWTilePointDesc *descs, const void *datas )
{
    if ( tile < 0 || tile >= 65536 )
        return false;

    if ( count <= 0 || !descs || !datas )
        return false;

    SPWTilesStorageData & m = *(SPWTilesStorageData *)m_ptr;
    if ( !m.m_pFile )
        return false;

    const PWUInt32 pointsCount = (PWUInt32)count;
    if ( !m.WriteTileData( tile, &pointsCount, 4 ) )
        return false;

    if ( !m.WriteTileData( tile, descs, pointsCount * 16 ) )
        return false;

    if ( !m.WriteTileData( tile, datas, count * m.m_itemDataSize ) )
        return false;

    return true;
}

//----------------------------------------------------------------

bool PWTilesStorage::BakeTile( int tile, int num_levels, TPWTileMixFunc func )
{
    if ( tile < 0 || tile >= 65536 )
        return false;

    if ( num_levels < 1 || num_levels > 30 )
        return false;

    SPWTilesStorageData & m = *(SPWTilesStorageData *)m_ptr;
    if ( !m.m_pFile || m.m_readOnly )
        return false;

    if ( !m.m_tilesDataPages[ tile ] )
    {
        if ( !m.InitTile( tile ) )
            return false;
    }

    if ( m.m_tilesDataStatus[ tile ] != PWTILE_NOT_BAKED )
        return false;

    PWUInt32 numPoints = 0;
    const PWUInt32 itemDataSize = m.m_itemDataSize;

    PWUInt32 offs = 0;
    for(;;)
    {
        PWUInt32 pointsCount = 0;
        if ( !m.ReadTileData( tile, offs, &pointsCount, 4 ) )
            break;

        numPoints += pointsCount;

        offs += 4;
        offs += pointsCount * (16 + itemDataSize);
    }

    PWArray<SPWTilePointDesc> descs;
    PWArray<char> datas;

    descs.Resize( numPoints );
    datas.Resize( itemDataSize * numPoints );

    PWUInt32 ptIdx = 0;
    offs = 0;
    for(;;)
    {
        PWUInt32 pointsCount = 0;
        if ( !m.ReadTileData( tile, offs, &pointsCount, 4 ) )
            break;

        if ( pointsCount == 0 )
            break;

        offs += 4;

        if ( !m.ReadTileData( tile, offs, &descs[ ptIdx ], pointsCount * 16 ) )
            return false;

        offs += pointsCount * 16;

        if ( !m.ReadTileData( tile, offs, &datas[ ptIdx * itemDataSize ], pointsCount * itemDataSize ) )
            return false;

        offs += pointsCount * itemDataSize;
        ptIdx += pointsCount;
    }

    // LATER: use_radiuses ~ realloc indices (and jump-moves-on-overlap & return grown size to parent!) ~ BakeTile2
    // LATER: fix_radiuses ~ check vs all points around and cut radius to nearest one (SLOW... but ok) ~ fix points

    if ( !m.BakeTile( num_levels, numPoints, &descs[0], &datas[0], func ) )
        goto err;

    if ( !SaveBakedTile( tile, num_levels ) )
        goto err;

    return true;
err:
    m.BakeTileCleanups();
    return false;
}

//----------------------------------------------------------------

bool PWTilesStorage::BakeTileFromMemory( int tile, int num_levels, TPWTileMixFunc func, int count, const SPWTilePointDesc *descs, const void *datas )
{
    if ( tile < 0 || tile >= 65536 )
        return false;

    if ( num_levels < 1 || num_levels > 30 )
        return false;

    if ( count <= 0 || !descs || !datas )
        return false;

    SPWTilesStorageData & m = *(SPWTilesStorageData *)m_ptr;
    if ( !m.m_pFile || m.m_readOnly )
        return false;

    PWUInt32 numPoints = count;
    const PWUInt32 itemDataSize = m.m_itemDataSize;

    if ( !m.BakeTile( num_levels, numPoints, descs, datas, func ) )
        goto err;

    if ( !SaveBakedTile( tile, num_levels ) )
        goto err;

    return true;
err:
    m.BakeTileCleanups();
    return false;
}

//----------------------------------------------------------------

bool PWTilesStorage::SaveBakedTile( int tile, int num_levels )
{
    SPWTilesStorageData & m = *(SPWTilesStorageData *)m_ptr;

    if ( !ClearTile( tile ) )
        goto err;

    // 8 PWTILE01
    // 1 0
    // 1 itemdatasize
    // 1 num_levels
    // 1 sum_block_size_shift
    // 4 reserved(0)
    // 
    // 4 offset of sums for level 0   ---> DWORD N, DWORD sum0 ... DWORD sumN-1
    // 4 offset of nodes for level 0  ---> DWORD LEN, ...
    // 4 offset of datas for level 0  ---> DWORD LEN, ...
    // 4 reserved (level 0)
    // ...

    if ( !m.WriteTileData( tile, "PWTILE01", 8 ) )
        goto err;

    PWUInt8 header[8];
    memset( header, 0, 8 );
    //
    header[1] = (PWUInt8)m.m_itemDataSize;
    header[2] = (PWUInt8)num_levels;
    header[3] = (PWUInt8)m.m_bake_sum_chunk_shift;
    //
    if ( !m.WriteTileData( tile, header, 8 ) )
        goto err;

    PWUInt32 table_size  = num_levels * 16;
    PWUInt32 data_offset = 16 + table_size;

    PWUInt32 offsets_sums[32];
    PWUInt32 offsets_octs[32];
    PWUInt32 offsets_data[32];

    for ( int i = 0; i < num_levels; ++i )
    {
        offsets_sums[i] = data_offset;
        data_offset += 4 + 4 * m.m_bake_sums[i].GetCount();
    }

    for ( int i = 0; i < num_levels; ++i )
    {
        offsets_octs[i] = data_offset;
        PWUInt32 sz = m.m_bake_sbuf[i].GetTotalSize();
        sz += (4-(sz%4)); // padding
        data_offset += 4 + sz;
    }

    for ( int i = 0; i < num_levels; ++i )
    {
        offsets_data[i] = data_offset;
        PWUInt32 sz = m.m_bake_dbuf[i].GetTotalSize();
        sz += (4-(sz%4)); // padding
        data_offset += 4 + sz;
    }

    PWUInt32 zero = 0;
    for ( int i = 0; i < num_levels; ++i )
    {
        if ( !m.WriteTileData( tile, &offsets_sums[i], 4 ) )
            goto err;

        if ( !m.WriteTileData( tile, &offsets_octs[i], 4 ) )
            goto err;

        if ( !m.WriteTileData( tile, &offsets_data[i], 4 ) )
            goto err;

        if ( !m.WriteTileData( tile, &zero, 4 ) )
            goto err;
    }

    for ( int i = 0; i < num_levels; ++i )
    {
        //if ( !m.VerifyWriteOffset( tile, offsets_sums[i] ) )
        //    return false;

        PWUInt32 count = m.m_bake_sums[i].GetCount();
        if ( !m.WriteTileData( tile, &count, 4 ) )
            goto err;

        if ( count > 0 )
        {
            if ( !m.WriteTileData( tile, &(m.m_bake_sums[i][0]), count*4 ) )
                goto err;
        }
    }

    char zbuf[4] = {0,0,0,0};

    for ( int i = 0; i < num_levels; ++i )
    {
        //if ( !m.VerifyWriteOffset( tile, offsets_octs[i] ) )
        //    return false;

        const PWBuffer & frombuf = m.m_bake_sbuf[i];

        const PWUInt32 sz = frombuf.GetTotalSize();
        if ( !m.WriteTileData( tile, &sz, 4 ) )
            goto err;

        const PWUInt32 blkcount = frombuf.GetBlocksCount();
        for ( PWUInt32 blkidx = 0; blkidx < blkcount; ++blkidx )
        {
            PWUInt32 block_sz;
            const char *ptr = frombuf.GetBlockData( blkidx, block_sz );
            if ( !ptr || block_sz == 0 )
                goto err;

            if ( !m.WriteTileData( tile, ptr, block_sz ) )
                goto err;
        }

        int padding = 4-(sz%4);
        if ( !m.WriteTileData( tile, zbuf, padding ) )
            goto err;
    }

    for ( int i = 0; i < num_levels; ++i )
    {
        //if ( !m.VerifyWriteOffset( tile, offsets_data[i] ) )
        //    return false;

        const PWBuffer & frombuf = m.m_bake_dbuf[i];

        const PWUInt32 sz = frombuf.GetTotalSize();
        if ( !m.WriteTileData( tile, &sz, 4 ) )
            goto err;

        const PWUInt32 blkcount = frombuf.GetBlocksCount();
        for ( PWUInt32 blkidx = 0; blkidx < blkcount; ++blkidx )
        {
            PWUInt32 block_sz;
            const char *ptr = frombuf.GetBlockData( blkidx, block_sz );
            if ( !ptr || block_sz == 0 )
                goto err;

            if ( !m.WriteTileData( tile, ptr, block_sz ) )
                goto err;
        }

        int padding = 4-(sz%4);
        if ( !m.WriteTileData( tile, zbuf, padding ) )
            goto err;
    }

    if ( !m.MarkTileBaked( tile ) )
        goto err;

    m.BakeTileCleanups();
    return GetTileStatus( tile ) == PWTILE_BAKED;

err:
    m.BakeTileCleanups();
    return false;
}

//----------------------------------------------------------------

bool PWTilesStorage::OptimizeTiles( IPWFile *pFileFrom, IPWFile *pFileTo )
{
    SPWTilesStorageData *pDataFrom = new SPWTilesStorageData();
    SPWTilesStorageData *pDataTo = new SPWTilesStorageData();

    char *buf = 0;
    PWUInt32 buf_sz = 0;

    if ( !pDataFrom->OpenTiles( pFileFrom, 0, true, true ) )
        goto err;

    if ( !pDataTo->NewTiles( pFileTo, pDataFrom->m_itemDataSize ) )
        goto err;

    for ( int tile = 0; tile < 65536; ++tile )
    {
        if ( pDataFrom->m_tilesPages[ tile ] == 0 )
            continue;

        if ( !pDataFrom->InitTile( tile ) )
            goto err;

        const PWUInt32 tiledata_sz = pDataFrom->m_tilesTotalDataSizes[ tile ];

        if ( !buf || buf_sz < tiledata_sz )
        {
            if ( buf ) delete[] buf;
            buf = new char[ tiledata_sz ];
            buf_sz = tiledata_sz;
        }

        if ( !pDataFrom->ReadTileData( tile, 0, buf, tiledata_sz ) )
            goto err;

        if ( !pDataTo->WriteTileData( tile, buf, tiledata_sz ) )
            goto err;

        if ( pDataFrom->m_tilesDataStatus[ tile ] == PWTILE_BAKED )
        {
            if ( !pDataTo->MarkTileBaked( tile ) )
                goto err;
        }
    }

    if ( buf ) delete[] buf;
    delete pDataFrom;
    delete pDataTo;
    return true;
err:
    if ( buf ) delete[] buf;
    delete pDataFrom;
    delete pDataTo;
    return false;
}

//----------------------------------------------------------------
