// pw_render01_test.h
#ifndef pw_render01_testH
#define pw_render01_testH

#include "../pw_tilescache.h"

//----------------------------------------------------------------

class PWRender01_Test
{
public:
    PWRender01_Test();
    ~PWRender01_Test();

public:
    void Reset();
    void Init( PWTilesCache *pCache, int bpp, void *dest, int pitch, int w, int h );

public:
    void Draw( int tile, float yaw, int max_level=0 );

private:
    void DrawRecursive( int tile, int level, PWUInt32 offset, PWUInt32 argb, float px, float py, float pz, float step );
    void DrawDot( int x, int y, const void *data );

private:
    PWTilesCache *m_pCache;

    int m_bpp;
    void *m_dest;
    int m_pitch;
    int m_w;
    int m_h;

    float m_ax;
    float m_ay;
    float m_bx;
    float m_by;
    float m_cx;
    float m_cy;
    float m_dx;
    float m_dy;

    int m_max_level;
};

//----------------------------------------------------------------

#endif
