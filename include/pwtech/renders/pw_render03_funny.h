// pw_render03_funny.h
#ifndef pw_render03_funnyH
#define pw_render03_funnyH

#include "../pw_tilescache.h"
#include "../pw_tilesmap.h"
#include "../pw_lookups.h"

#include "../types/pw_vec3.h"
#include "../types/pw_transform3.h"

#include "../wraps/pw_math.h"

#include "nodes/pw_rendernodes2.h"

#include <memory.h>
#include <string.h>

#pragma warning ( disable : 4005 ) // macro redefinition

//----------------------------------------------------------------

template<int ItemDataSize>
class PWRender03_Funny
{
public:
    PWRender03_Funny()
    {
        m_state = STATE_NOT_INIT;
    }

    ~PWRender03_Funny()
    {
    }

public:
    bool Setup( int width, int height,
                int scr_pitch, void *dbuf, PWInt32 *zbuf,
                PWTilesMap<ItemDataSize> *pMap,
                PWTilesCache *pCache, int tileShift )
    {
        if ( width < 1 || height < 1 || !dbuf || !zbuf || !pMap || !pCache )
        {
            m_state = STATE_ERROR;
            return false;
        }

        m_w = width;
        m_h = height;

        m_scr_pitch = scr_pitch;
        m_dbuf = dbuf;
        m_zbuf = zbuf;

        m_pMap      = pMap;
        m_pCache    = pCache;
        m_tileShift = tileShift;

        m_nodes.Init( m_w * m_h * 2 );

        m_stat_nodes_touched = 0;

        m_state = STATE_MAIN;
        return true;
    }

    void Flush()
    {
        m_nodes.Clear();
    }

public:
    int GetWidth () const { return m_w; }
    int GetHeight() const { return m_h; }

    int GetStat_NodesTouched     () const { return m_stat_nodes_touched; }
    int GetStat_NodesChecked     () const { return m_stat_nodes_checked; }
    int GetStat_NodesDrawn       () const { return m_stat_nodes_drawn; }
    int GetStat_NodesNoClipZ     () const { return m_stat_nodes_noclipz; }
    int GetStat_NodesNoClipXY    () const { return m_stat_nodes_noclipxy; }
    int GetStat_NodesOrthoTouched() const { return m_stat_nodes_ortho_touched; }
    int GetStat_NodesOrthoChecked() const { return m_stat_nodes_ortho_checked; }
    int GetStat_NodesOrthoDrawn  () const { return m_stat_nodes_ortho_drawn; }

private:
    enum EState
    {
        STATE_NOT_INIT,
        STATE_MAIN,
        STATE_ERROR,
    };

    EState                          m_state;

    int                             m_w;
    int                             m_h;

    int                             m_scr_pitch;
    void                           *m_dbuf;
    PWInt32                        *m_zbuf;

    PWTilesMap<ItemDataSize>       *m_pMap;
    PWTilesCache                   *m_pCache;
    int                             m_tileShift;

    typedef SPWRenderNode2<ItemDataSize> TNode;

    PWRenderNodes2<ItemDataSize>    m_nodes;

    Transform3D                     m_cam_tr;

    struct SStackItem
    {
        PWUInt32 rd_idx_and_depth;
        PWInt32  px, py, pz;
        PWInt32  tx, ty, tz;
        PWUInt32 wr_idx;
        PWUInt8 tmp1;
    };

    SStackItem                      m_stack[ 32*8 ];
    int                             m_stack_pos;

    PWInt32                         m_dist_ahead  [ 32 ];
    PWInt32                         m_dist_radius2[ 32 ];

    PWLUT_OctOrders                 m_lut_oct_orders;
    PWLUT_ByteCounts                m_lut_byte_counts;

    PWInt32                         m_tr_delta_x[ 32*8 ];
    PWInt32                         m_tr_delta_y[ 32*8 ];
    PWInt32                         m_tr_delta_z[ 32*8 ];

    PWInt32                         m_tr_x0;
    PWInt32                         m_tr_y0;
    PWInt32                         m_tr_kx;
    PWInt32                         m_tr_ky;

    PWInt32                         m_tr_x0_64;
    PWInt32                         m_tr_y0_64;
    PWInt32                         m_tr_kx_64;
    PWInt32                         m_tr_ky_64;

    PWUInt32                        m_tr_psz;
    PWUInt32                        m_tr_qsz;

    PWInt32                         m_ortho_dir_m8;
    PWInt32                         m_ortho_vx[8];
    PWInt32                         m_ortho_vy[8];
    PWInt32                         m_ortho_vz[8];
    PWInt32                         m_ortho_sx;
    PWInt32                         m_ortho_sy;

    int                             m_tile;

    int                             m_stat_nodes_touched;
    int                             m_stat_nodes_checked;
    int                             m_stat_nodes_drawn;
    int                             m_stat_nodes_noclipz;
    int                             m_stat_nodes_noclipxy;
    int                             m_stat_nodes_ortho_touched;
    int                             m_stat_nodes_ortho_checked;
    int                             m_stat_nodes_ortho_drawn;

    PWArray<PWUInt32>               m_tmp_path;

private:
    float WrapF( float x, float m )
    {
        if ( x < 0.0f ) return m - fmodf( -x, m );
        return fmodf( x, m );
    }

public:
    void Render( bool clear_dbuf, bool clear_zbuf,
                 float proj_x0, float proj_y0, float proj_kx, float proj_ky,
                 const PWVec3 & pos, float yaw, float pitch, float roll )
    {
        m_stat_nodes_touched        = 0;
        m_stat_nodes_checked        = 0;
        m_stat_nodes_drawn          = 0;
        m_stat_nodes_noclipz        = 0;
        m_stat_nodes_noclipxy       = 0;
        m_stat_nodes_ortho_touched  = 0;
        m_stat_nodes_ortho_checked  = 0;
        m_stat_nodes_ortho_drawn    = 0;

        if ( m_state != STATE_MAIN )
            return;

        if ( clear_dbuf )
        {
            memset( m_dbuf, 0, m_scr_pitch * m_h * ItemDataSize );
        }

        if ( clear_zbuf )
        {
            memset( m_zbuf, 0, m_scr_pitch * m_h * 4 );
        }

        PWUInt32 msz;
        PWInt32 mdx, mdy, mdz;
        const PWUInt32 root_idx = m_pMap->GetRoot( mdx, mdy, mdz, msz );
        if ( !root_idx )
            return;

        const float kpos = (float)( 1 << m_tileShift );
        PWVec3 mpos( pos.x * kpos, pos.y * kpos, pos.z * kpos );
        m_cam_tr.SetYPRInverse( mpos, yaw, pitch, roll );

        const PWUInt32 mhalf = (msz << m_tileShift ) >> 1;
        const PWInt32 px = mhalf - ( mdx << m_tileShift );
        const PWInt32 py = mhalf - ( mdy << m_tileShift );
        const PWInt32 pz = mhalf - ( mdz << m_tileShift );

        double fx = (double)px;
        double fy = (double)py;
        double fz = (double)pz;
        m_cam_tr.Apply( fx, fy, fz );

        const int tx = (int)floor( fx );
        const int ty = (int)floor( fy );
        const int tz = (int)floor( fz );

        m_tr_x0 = (int)floorf( proj_x0 );
        m_tr_y0 = (int)floorf( proj_y0 );
        m_tr_kx = (int)floorf( proj_kx );
        m_tr_ky = (int)floorf( proj_ky );

        m_tr_x0_64 = m_tr_x0 << 6;
        m_tr_y0_64 = m_tr_y0 << 6;
        m_tr_kx_64 = m_tr_kx << 6;
        m_tr_ky_64 = m_tr_ky << 6;

        m_tr_psz = msz << m_tileShift;
        m_tr_qsz = m_tr_psz >> 2;

        const float k_pi2 = 3.141592f * 0.50f;
        const float k_pi4 = 3.141592f * 0.25f;
        float sub_yaw   = WrapF( yaw,   k_pi2 );
        float sub_pitch = WrapF( pitch, k_pi2 );
        if ( sub_yaw   > k_pi4 ) sub_yaw   = k_pi2 - sub_yaw;
        if ( sub_pitch > k_pi4 ) sub_pitch = k_pi2 - sub_pitch;
        sub_yaw   = k_pi4 - sub_yaw;
        sub_pitch = k_pi4 - sub_pitch;

        const PWInt32 kmax = (PWInt32)( ( m_tr_kx >= m_tr_ky ) ? m_tr_kx : m_tr_ky );
        const int kxrot = (int)floorf( cosf(sub_yaw  ) * 1.414213562f * 4096 );
        const int kyrot = (int)floorf( cosf(sub_pitch) * 1.414213562f * 4096 );
        //
        for ( int depth = 0; depth < 30; ++depth )
        {
            PWInt64 tmp;

            tmp = m_tr_psz; tmp *= kmax; tmp >>= depth; tmp *= 3; tmp /= 2;
            if ( tmp > 0x7FFFFFFFUL ) tmp = 0x7FFFFFFFUL;
            m_dist_ahead[ depth ] = (PWInt32)tmp;

            tmp = m_tr_psz; tmp >>= depth;
            if ( tmp > 0x7FFFFFFFUL ) tmp = 0x7FFFFFFFUL;
            m_dist_radius2[ depth ] = (PWInt32)tmp;
        }

        const PWInt32 mquart = (PWInt32)( mhalf >> 1 );
        for ( int oct_idx = 0; oct_idx < 8; ++oct_idx )
        {
            double fx2 = (double)px + ( (oct_idx & 1) ? mquart : -mquart );
            double fy2 = (double)py + ( (oct_idx & 2) ? mquart : -mquart );
            double fz2 = (double)pz + ( (oct_idx & 4) ? mquart : -mquart );
            
            m_cam_tr.Apply( fx2, fy2, fz2 );

            fx2 -= fx;
            fy2 -= fy;
            fz2 -= fz;

            m_tr_delta_x[ oct_idx ] = (int)floor( fx2 );
            m_tr_delta_y[ oct_idx ] = (int)floor( fy2 );
            m_tr_delta_z[ oct_idx ] = (int)floor( fz2 );
        }

        for ( int depth = 1; depth < 30; ++depth )
        {
            int ofs_from = ((depth-1)<<3);
            int ofs_to   = (depth    <<3);

            for ( int oct_idx = 0; oct_idx < 8; ++oct_idx )
            {
                m_tr_delta_x[ ofs_to ] = m_tr_delta_x[ ofs_from ] >> 1;
                m_tr_delta_y[ ofs_to ] = m_tr_delta_y[ ofs_from ] >> 1;
                m_tr_delta_z[ ofs_to ] = m_tr_delta_z[ ofs_from ] >> 1;

                ++ofs_to;
                ++ofs_from;
            }
        }

        const PWInt32 cmx = (PWInt32)floorf( mpos.x );
        const PWInt32 cmy = (PWInt32)floorf( mpos.y );
        const PWInt32 cmz = (PWInt32)floorf( mpos.z );

        if ( m_nodes.ReadNode(0)->src != root_idx )
        {
            const PWTilesMap<ItemDataSize>::SNode *pMapNode = m_pMap->GetNode( root_idx );

            m_nodes.Clear();

            PWUInt32 tmp_idx;
            SPWRenderNode2<ItemDataSize> *pNode = m_nodes.WriteNodes( tmp_idx, 1 );

            pNode->oct          = 0;
            pNode->level_flags  = 0; // level 0, flags 0
            pNode->tile         = ( pMapNode->oct ? 0 : pMapNode->tile );

            pNode->src          = root_idx;
            pNode->idx_children = 0;

            memcpy( pNode->data, pMapNode->data, ItemDataSize );

            m_nodes.Swap();
        }

        m_stack_pos = 0;
        SStackItem & item = m_stack[0];

        item.rd_idx_and_depth = 0;
        item.px = px - cmx;
        item.py = py - cmy;
        item.pz = pz - cmz;
        item.tx = tx;
        item.ty = ty;
        item.tz = tz;
        item.wr_idx = 0;

        const TNode *pReadRootNode = m_nodes.ReadNode(0);
        TNode *pWriteRootNode = m_nodes.WriteNodes( item.wr_idx, 1 );
        if ( !pWriteRootNode )
            return;

        memcpy( pWriteRootNode, pReadRootNode, sizeof(TNode) );
        //
        pWriteRootNode->level_flags &= ~0x80;
        //pWriteRootNode->idx_children = 0;
        //pWriteRootNode->oct = 0;

        RenderMapNode_1100( 1 );

        m_nodes.Swap();
    }

private:
    //#define PWR_DEBUG
    #define PWR_STAT
    #define PWR_TEST
    
    //#define PWR_OPTS
    #define PWR_TRICKS
    //#define PWR_OPT4B
    //#define PWR_OPT_ORTHOSAMEZ

    //#define PWR_NOORTHO
    #define PWR_NOTILES

    #define PWR_FLAG_Z      0x01
    #define PWR_FLAG_XY     0x02
    #define PWR_FLAG_ORTHO  0x04

    //------------------------------------------
    // MAP NODE FUNCS
    //------------------------------------------

    #define PWR_CLIPZ  1
    #define PWR_CLIPXY 1
    #define PWR_ORTHO  0
    #define PWR_TILE   0
    #include "parts/pw_render03_func.h"

    #define PWR_CLIPZ  0
    #define PWR_CLIPXY 1
    #define PWR_ORTHO  0
    #define PWR_TILE   0
    #include "parts/pw_render03_func.h"

    #define PWR_CLIPZ  0
    #define PWR_CLIPXY 0
    #define PWR_ORTHO  0
    #define PWR_TILE   0
    #include "parts/pw_render03_func.h"

    #define PWR_CLIPZ  0
    #define PWR_CLIPXY 0
    #define PWR_ORTHO  1
    #define PWR_TILE   0
    #include "parts/pw_render03_func.h"

    //------------------------------------------
    // TILE NODE FUNCS
    //------------------------------------------

    #define PWR_CLIPZ  1
    #define PWR_CLIPXY 1
    #define PWR_ORTHO  0
    #define PWR_TILE   1
    #include "parts/pw_render03_func.h"

    #define PWR_CLIPZ  0
    #define PWR_CLIPXY 1
    #define PWR_ORTHO  0
    #define PWR_TILE   1
    #include "parts/pw_render03_func.h"

    #define PWR_CLIPZ  0
    #define PWR_CLIPXY 0
    #define PWR_ORTHO  0
    #define PWR_TILE   1
    #include "parts/pw_render03_func.h"

    #define PWR_CLIPZ  0
    #define PWR_CLIPXY 0
    #define PWR_ORTHO  1
    #define PWR_TILE   1
    #include "parts/pw_render03_func.h"
};

//----------------------------------------------------------------

#endif
