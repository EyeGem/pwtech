// pw_render01_test.cpp
#include "pw_render01_test.h"

#include <memory.h>
#include <string.h>

#include <math.h>

//----------------------------------------------------------------

PWRender01_Test::PWRender01_Test()
{
    Reset();
}

//----------------------------------------------------------------

PWRender01_Test::~PWRender01_Test()
{
}

//----------------------------------------------------------------

void PWRender01_Test::Reset()
{
    m_pCache = 0;

    m_bpp    = 1;
    m_dest   = 0;
    m_pitch  = 0;
    m_w      = 1;
    m_h      = 1;
}

//----------------------------------------------------------------

void PWRender01_Test::Init( PWTilesCache *pCache, int bpp, void *dest, int pitch, int w, int h )
{
    Reset();

    if ( bpp > 16 )
        return;

    m_pCache = pCache;

    m_bpp    = bpp;
    m_dest   = dest;
    m_pitch  = pitch;
    m_w      = w;
    m_h      = h;
}

//----------------------------------------------------------------

void PWRender01_Test::Draw( int tile, float yaw, int max_level )
{
    if ( !m_pCache || !m_dest )
        return;

    m_max_level = max_level;
    if ( m_max_level <= 0 )
        m_max_level = 30;

    const float sk = m_w * 0.3f;

    m_ax = sk *  cosf(yaw);
    m_ay = sk * -sinf(yaw) * 0.3f;

    m_bx = sk * -sinf(yaw);
    m_by = sk * -cosf(yaw) * 0.3f;

    m_cx = sk *  0.0f;
    m_cy = sk * -1.0f;

    m_dx = m_w * 0.5f;
    m_dy = m_h * 0.5f;

    DrawRecursive( tile, 0, 0, 0xFF000000, 0.0f, 0.0f, 0.0f, 0.25f );
}

//----------------------------------------------------------------

void PWRender01_Test::DrawRecursive( int tile, int level, PWUInt32 offset, PWUInt32 argb, float px, float py, float pz, float step )
{
    if ( level < m_max_level )
    {
        bool wait;
        const SPWTilesCacheSubdiv *subdiv = m_pCache->Subdiv( wait, tile, level, offset );
        if ( subdiv )
        {
            PWUInt8 oct = subdiv->oct;
            if ( oct )
            {
                PWUInt32 off = subdiv->offset;
                int idx = 0;

                const float step2 = step * 0.5f;

                PWUInt32 colors[8];
                memcpy( colors, subdiv->datas, 8 * 4 );

                if ( oct & 0x01 ) { DrawRecursive( tile, level+1, off+idx, colors[idx], px - step, py - step, pz - step, step2 ); ++idx; }
                if ( oct & 0x02 ) { DrawRecursive( tile, level+1, off+idx, colors[idx], px + step, py - step, pz - step, step2 ); ++idx; }
                if ( oct & 0x04 ) { DrawRecursive( tile, level+1, off+idx, colors[idx], px - step, py + step, pz - step, step2 ); ++idx; }
                if ( oct & 0x08 ) { DrawRecursive( tile, level+1, off+idx, colors[idx], px + step, py + step, pz - step, step2 ); ++idx; }
                if ( oct & 0x10 ) { DrawRecursive( tile, level+1, off+idx, colors[idx], px - step, py - step, pz + step, step2 ); ++idx; }
                if ( oct & 0x20 ) { DrawRecursive( tile, level+1, off+idx, colors[idx], px + step, py - step, pz + step, step2 ); ++idx; }
                if ( oct & 0x40 ) { DrawRecursive( tile, level+1, off+idx, colors[idx], px - step, py + step, pz + step, step2 ); ++idx; }
                if ( oct & 0x80 ) { DrawRecursive( tile, level+1, off+idx, colors[idx], px + step, py + step, pz + step, step2 ); ++idx; }

                return;
            }
        }
    }

    const float sx = m_dx + m_ax * px + m_bx * py + m_cx * pz;
    const float sy = m_dy + m_ay * px + m_by * py + m_cy * pz;

    //const PWUInt32 argb = 0xFFFF0000; //0xFF00FF00 + level*8;
    DrawDot( (int)floorf(sx), (int)floorf(sy), &argb );
}

//----------------------------------------------------------------

void PWRender01_Test::DrawDot( int x, int y, const void *data )
{
    if ( x < 0 || x >= m_w ) return;
    if ( y < 0 || y >= m_h ) return;
    if ( !data ) return;

    memcpy( (PWUInt8 *)m_dest + m_bpp * (y * m_pitch + x), data, m_bpp );
}

//----------------------------------------------------------------
