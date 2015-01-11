// pw_tilescache.h
#ifndef pw_tilesscacheH
#define pw_tilesscacheH

#include "types/pw_ints.h"

//----------------------------------------------------------------

struct SPWTilesCacheRequest
{
    int      tile;
    PWUInt32 offset;
    PWUInt32 size;
};

struct SPWTilesCacheSubdiv
{
    PWUInt8     oct;
    const void *datas;
    PWUInt32    offset;
};

//----------------------------------------------------------------

class PWTilesCache
{
private: // NO COPY
    PWTilesCache( const PWTilesCache & );
    void operator = ( const PWTilesCache & );

public:
    PWTilesCache();
    ~PWTilesCache();

public:
    const SPWTilesCacheSubdiv *Subdiv( bool & wait, int tile, int level, PWUInt32 offset );

public:
    const SPWTilesCacheRequest *GetRequests( int & count );
    void GotRequests();

    void OnLoaded( const SPWTilesCacheRequest & request, const void *data );
    void OnFailed( const SPWTilesCacheRequest & request );

private:
    void *m_ptr;
};

//----------------------------------------------------------------

#endif
