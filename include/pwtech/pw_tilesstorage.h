// pw_tilesstorage.h
#ifndef pw_tilesstorageH
#define pw_tilesstorageH

#include "wraps/pw_file.h"
#include "types/pw_array.h"

//----------------------------------------------------------------

struct SPWTilePointDesc
{
    float x,y,z;  // coords [0..1] only inside tile cube
    float radius; // optional baking parameter (big points)
};

enum EPWTileStatus
{
    PWTILE_NOTINIT,
    PWTILE_EMPTY,
    PWTILE_ERROR,
    PWTILE_NOT_BAKED,
    PWTILE_BAKED,
};

typedef void (*TPWTileMixFunc)( void *dest, const void *src, int count, int level );

//----------------------------------------------------------------

class PWTilesStorage
{
private: // NO COPY
    PWTilesStorage( const PWTilesStorage & );
    void operator = ( const PWTilesStorage & );

public:
    PWTilesStorage();
    ~PWTilesStorage();

public:
    bool NewTiles( IPWFile *pFile, PWUInt8 itemDataSize );
    bool OpenTiles( IPWFile *pFile, PWUInt8 itemDataSize, bool readOnly = true );
    bool CloseTiles();

public:
    bool ReadBakedTile( int tile, PWUInt32 offset, void *dest, PWUInt32 size );
    bool ReadBakedTileRootItemData( int tile, void *dest, PWUInt8 itemDataSize );

public:
    EPWTileStatus GetTileStatus( int tile );

    bool ClearTile( int tile );
    bool WriteTilePoints( int tile, int count, const SPWTilePointDesc *descs, const void *datas );
    bool BakeTile( int tile, int num_levels, TPWTileMixFunc func );

    bool BakeTileFromMemory( int tile, int num_levels, TPWTileMixFunc func, int count, const SPWTilePointDesc *descs, const void *datas );

public:
    static bool OptimizeTiles( IPWFile *pFileFrom, IPWFile *pFileTo );

private:
    bool SaveBakedTile( int tile, int num_levels );
    void *m_ptr;
};

//----------------------------------------------------------------

#endif
