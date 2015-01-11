// part

//----------------------------------------------------------------

#define PWR_JOIN_(X,Y) X##Y
#define PWR_JOIN(X,Y) PWR_JOIN_(X,Y)

#undef  PWR_FUNC
#define PWR_FUNC PWR_JOIN( PWR_JOIN( PWR_JOIN( PWR_JOIN( RenderMapNode_, PWR_CLIPZ ), PWR_CLIPXY ), PWR_ORTHO ), PWR_TILE )

#undef  PWR_MULDIV
//#define PWR_MULDIV( aaa, bbb, ccc ) (PWInt32)(((aaa) * (PWInt64)(bbb)) / (ccc))
#define PWR_MULDIV( aaa, bbb, ccc ) PWMulDiv32( (aaa), (bbb), (ccc) )

#define PWR_SCROFS( sx, sy ) ((sy) * m_scr_pitch + (sx))

#define PWR_TNODE TNode

//----------------------------------------------------------------

void PWR_FUNC( int stack_count )
{
    #ifdef PWR_TEST
    static PWUInt32 test_colors[32] ={
        0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0xFFFFFF00, 0xFF00FFFF, 0xFFFFFFFF, 0xFF888888, 0xFF333333,
        0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0xFFFFFF00, 0xFF00FFFF, 0xFFFFFFFF, 0xFF888888, 0xFF333333,
        0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0xFFFFFF00, 0xFF00FFFF, 0xFFFFFFFF, 0xFF888888, 0xFF333333,
        0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0xFFFFFF00, 0xFF00FFFF, 0xFFFFFFFF, 0xFF888888, 0xFF333333,
    };
    #endif

    #if PWR_ORTHO == 0
    int     draw_sx, draw_sy, draw_sw, draw_sh;
    int     draw_ofs;
    PWInt32 draw_zval;
    bool    big, can_draw;
    #endif

    int stack_start = m_stack_pos - stack_count;
    while ( m_stack_pos > stack_start )
    {
        #if PWR_ORTHO == 0
            #ifdef PWR_STAT
            #if PWR_CLIPZ == 0
            ++m_stat_nodes_noclipz;
            #endif
            #if PWR_CLIPXY == 0
            ++m_stat_nodes_noclipxy;
            #endif
            #endif

            #ifdef PWR_STAT
            ++m_stat_nodes_touched;
            #endif
        #else
            #ifdef PWR_STAT
            ++m_stat_nodes_ortho_touched;
            #endif
        #endif

        SStackItem & item = m_stack[ m_stack_pos ];
        --m_stack_pos;

        const PWR_TNODE *pNode = m_nodes.ReadNode( item.rd_idx_and_depth & 0x07FFFFFFUL );

        #if PWR_ORTHO == 0
            can_draw = false;

            #ifdef PWR_OPTS
            int flags = 0;
            #endif

            const PWInt32 tz = item.tz;
            const int depth = (int)((item.rd_idx_and_depth & 0xF8000000UL) >> 27);

            #if PWR_CLIPZ == 1
            const PWInt32 distRadius2 = m_dist_radius2[ depth ];
            if ( tz <= distRadius2 )
            {
                if ( tz < -distRadius2 )
                    continue;

                const PWInt32 distRadiusOut = distRadius2 + (distRadius2 << 1);
                const PWInt32 tx = item.tx; if ( tx < -distRadiusOut || tx > distRadiusOut ) continue;
                const PWInt32 ty = item.ty; if ( ty < -distRadiusOut || ty > distRadiusOut ) continue;

                goto do_children;
            }
            #ifdef PWR_OPTS
            flags |= PWR_FLAG_Z;
            #endif
            #endif
        
            const PWInt32 distAhead = m_dist_ahead[ depth ];
            if ( tz >= distAhead )
                goto do_draw_small;

            {
                const PWUInt32 i_skip = (~( ((item.px >> (31-0)) & 1) | ((item.py >> (30-0)) & 2) | ((item.pz >> (29-0)) & 4) )) & 7;

                int sx1,sx2,sy1,sy2;
                int cnt = 0;

                const int base_ofs = (((int)depth-1) << 3);
                for ( int i = 0; i < 8; ++i )
                {
                    if ( i == i_skip )
                        continue;

                    const int ofs = base_ofs | i;
                    const PWInt32 tx1 = item.tx + m_tr_delta_x[ ofs ];
                    const PWInt32 ty1 = item.ty + m_tr_delta_y[ ofs ];
                    const PWInt32 tz1 = item.tz + m_tr_delta_z[ ofs ];

                    if ( tz1 <= 0 )
                        continue;

                    const int sx =  PWR_MULDIV( tx1, m_tr_kx, tz1 );
                    const int sy = -PWR_MULDIV( ty1, m_tr_ky, tz1 );

                    if ( !cnt )
                    {
                        sx1 = sx2 = sx;
                        sy1 = sy2 = sy;
                        cnt = 1;
                    }
                    else
                    {
                        ++cnt;
                        if ( sx < sx1 ) sx1 = sx;
                        if ( sx > sx2 ) sx2 = sx;
                        if ( sy < sy1 ) sy1 = sy;
                        if ( sy > sy2 ) sy2 = sy;
                    }
                }

                if ( cnt <= 1 )
                    goto do_children;

                draw_sw = sx2 - sx1 + 1;
                draw_sh = sy2 - sy1 + 1;
                draw_sx = m_tr_x0 + sx1;
                draw_sy = m_tr_y0 + sy1;
            }

            big = ( draw_sw > 2 || draw_sh > 2 );

            #if PWR_CLIPXY == 1
            {
                bool inside = true;

                int tmp;
                if ( draw_sx < 0 ) { draw_sw += draw_sx; draw_sx=0; inside = false; } tmp = m_w-(draw_sx + draw_sw); if ( tmp < 0 ) { draw_sw += tmp; inside = false; }
                if ( draw_sy < 0 ) { draw_sh += draw_sy; draw_sy=0; inside = false; } tmp = m_h-(draw_sy + draw_sh); if ( tmp < 0 ) { draw_sh += tmp; inside = false; }
                
                if ( draw_sw <= 0 || draw_sh <= 0 )
                    continue;

                #ifdef PWR_OPTS
                const int safe_dist = 2;
                if ( inside && draw_sx >= safe_dist && draw_sy >= safe_dist && draw_sx+draw_sw < m_w-safe_dist && draw_sy+draw_sh <= m_h-safe_dist )
                    flags |= PWR_FLAG_XY;
                #endif
            }
            #endif

            can_draw = true;

            //=========================================================================
            //=========================================================================
            #ifdef PWR_OPTS
            #ifndef PWR_NOORTHO
            #if PWR_ORTHO == 0 && PWR_CLIPZ == 0 && PWR_CLIPXY == 0
            if ( draw_sw <= 32 && draw_sh <= 32 ) //&& ( !PWR_CLIPZ || (flags & PWR_FLAG_Z) ) && ( !PWR_CLIPXY || (flags & PWR_FLAG_XY) ) )
                flags |= PWR_FLAG_ORTHO;
            #endif
            #endif
            #endif
            //=========================================================================
            //=========================================================================

            #ifdef PWR_STAT
            ++m_stat_nodes_checked;
            #endif

            {
                const PWInt32 zval = 0x7FFFFFFF - tz;

                draw_ofs  = PWR_SCROFS( draw_sx, draw_sy );
                draw_zval = zval;
                
                int offset = draw_ofs;
                for ( int cy = 0; cy < draw_sh; ++cy )
                {
                    int ofs = offset;

                    for ( int cx = 0; cx < draw_sw; ++cx )
                    {
                        #ifdef PWR_DEBUG
                        if ( ofs < 0 )
                            continue;
                        #endif

                        if ( m_zbuf[ ofs ] < zval )
                            goto do_seen;

                        ++ofs;
                    }

                    offset += m_scr_pitch;
                }
            }
        #endif

        #if PWR_ORTHO == 1
            // TODO
        #endif

        continue;

    do_seen:
        #if PWR_ORTHO == 0
            if ( !big )
                goto do_draw_big;
        #else
            // TODO
        #endif

    do_children:
        if ( (pNode->level_flags & 0x80) == 0 )
        {
            #if PWR_TILE == 0
                if ( pNode->tile )
                {
                    #ifndef PWR_NOTILES
                    goto do_tile_children;
                    #endif

                    PWR_TNODE *pWriteNode = m_nodes.ModifyNode( item.wr_idx );
                    pWriteNode->idx_children = 0;
                    pWriteNode->level_flags = pNode->level_flags | 0x80;
                    pWriteNode->oct = 0;
                }
                else
                {
                    PWR_TNODE *pWriteNode = m_nodes.ModifyNode( item.wr_idx );
                    pWriteNode->idx_children = 0;
                    pWriteNode->level_flags = pNode->level_flags | 0x80;
                    pWriteNode->oct = 0;

                    PWTilesMap<ItemDataSize>::SNode *pMapNode = m_pMap->GetNode( pNode->src );
                    if ( pMapNode->oct && pMapNode->idx_children )
                    {
                        const int new_count = m_lut_byte_counts.lookup[ pMapNode->oct ];

                        PWUInt32 new_idx;
                        PWR_TNODE *pNewNodes = m_nodes.WriteNodes( new_idx, new_count );

                        if ( pNewNodes )
                        {
                            pWriteNode->oct = pMapNode->oct;
                            pWriteNode->idx_children = new_idx;

                            const PWUInt8 new_level = (pNode->level_flags & 0x1F) + 1; // level +1, flags 0

                            for ( int i = 0; i < new_count; ++i )
                            {
                                const PWUInt32 child_src = pMapNode->idx_children + i;
                                PWTilesMap<ItemDataSize>::SNode *pChildMapNode = m_pMap->GetNode( child_src );
                                
                                PWR_TNODE *pNewNode = (pNewNodes + i);

                                pNewNode->src = child_src;
                                pNewNode->idx_children = 0;

                                pNewNode->level_flags = new_level;
                                pNewNode->oct = 0;
                                pNewNode->tile = ( pChildMapNode->oct ? 0 : pChildMapNode->tile );

                                memcpy( pNewNode->data, pChildMapNode->data, ItemDataSize );
                            }
                        }
                    }
                }                

                if ( can_draw )
                    goto do_draw_big;
                continue;
            #else
                // TODO tile subdiv
            #endif
        }

        const int node_oct = (int)pNode->oct;
        if ( !node_oct )
        {
            PWR_TNODE *pWriteNode = m_nodes.ModifyNode( item.wr_idx );
            pWriteNode->level_flags = pNode->level_flags | 0x80;
            //pWriteNode->idx_children = 0;
            //pWriteNode->oct = 0;

            if ( can_draw )
                goto do_draw_big;
            continue;
        }

        const int children_count = m_lut_byte_counts.lookup[ node_oct ];
        PWUInt32 wr_idx_base;
        PWR_TNODE *pWriteNodes = m_nodes.WriteNodes( wr_idx_base, children_count );
        if ( !pWriteNodes )
            continue;

        PWR_TNODE *pWriteNode = m_nodes.ModifyNode( item.wr_idx );
        pWriteNode->idx_children = wr_idx_base;
        pWriteNode->level_flags = pNode->level_flags; // | 0x80;
        pWriteNode->oct = pNode->oct;

        const PWUInt32 dir_m8         = ((item.px >> (31-3)) & 8) | ((item.py >> (30-3)) & 16) | ((item.pz >> (29-3)) & 32);
        const PWUInt32 idx_base       = ( node_oct << 6 ) | dir_m8;
        const PWUInt32 idx_child      = pNode->idx_children;
        const int     *orders         = &m_lut_oct_orders.lookup[ idx_base ];

        const PWR_TNODE *pReadNodes = m_nodes.ReadNode( idx_child );
        memcpy( pWriteNodes, pReadNodes, children_count * sizeof(PWR_TNODE) );

        for ( int i = 0; i < children_count; ++i )
        {
            (pWriteNodes+i)->level_flags = (pReadNodes+i)->level_flags & ~0x80;
            //(pWriteNodes+i)->idx_children = 0;
            //(pWriteNodes+i)->oct = 0;
        }

        const PWUInt32 and_next_depth = (((PWUInt32)depth)+1) << 27;
        const PWInt32  half           = m_tr_qsz >> depth;
        const int      base_ofs       = (((int)depth) << 3);

        const PWInt32 pxs[2] = { item.px - half, item.px + half };
        const PWInt32 pys[2] = { item.py - half, item.py + half };
        const PWInt32 pzs[2] = { item.pz - half, item.pz + half };

        const PWInt32 itx = item.tx;
        const PWInt32 ity = item.ty;
        const PWInt32 itz = item.tz;

        for ( int i = children_count-1; i >= 0; --i )
        {
            const int order = orders[i];
            const int child_ofs = order & 255;
            const int oct_idx   = order >> 8;

            SStackItem & child = m_stack[ ++m_stack_pos ];

            child.rd_idx_and_depth = (idx_child + child_ofs) | and_next_depth;

            child.px = pxs[ (oct_idx & 1)      ];
            child.py = pys[ (oct_idx & 2) >> 1 ];
            child.pz = pzs[ (oct_idx & 4) >> 2 ];

            child.tmp1 = oct_idx;
            
            const int ofs = base_ofs | oct_idx;
            child.tx = itx + m_tr_delta_x[ ofs ];
            child.ty = ity + m_tr_delta_y[ ofs ];
            child.tz = itz + m_tr_delta_z[ ofs ];

            child.wr_idx = wr_idx_base + child_ofs;
        }

        #ifdef PWR_OPTS
        #if PWR_TILE == 0
            #if PWR_CLIPZ == 1 && PWR_CLIPXY == 1 && PWR_ORTHO == 0
            if      ( flags & PWR_FLAG_XY    ) RenderMapNode_0000( children_count );
            else if ( flags & PWR_FLAG_Z     ) RenderMapNode_0100( children_count );
            #elif PWR_CLIPZ == 0 && PWR_CLIPXY == 1 && PWR_ORTHO == 0
            if      ( flags & PWR_FLAG_XY    ) RenderMapNode_0000( children_count );
            #endif
        #else
            #if PWR_CLIPZ == 1 && PWR_CLIPXY == 1 && PWR_ORTHO == 0
            if      ( flags & PWR_FLAG_XY    ) RenderMapNode_0001( children_count );
            else if ( flags & PWR_FLAG_Z     ) RenderMapNode_0101( children_count );
            #elif PWR_CLIPZ == 0 && PWR_CLIPXY == 1 && PWR_ORTHO == 0
            if      ( flags & PWR_FLAG_XY    ) RenderMapNode_0001( children_count );
            #endif
        #endif
        #endif

        continue;

    #ifndef PWR_NOTILES
    do_tile_children:
        // TODO from map to tile nodes
        continue;
    #endif

    //    PWUInt32 node_oct = (int)pNode->oct;
    //    if ( !node_oct )
    //    {
    //        #if PWR_TILE == 0
    //            #ifndef PWR_NOTILES
    //            goto do_tile_children;
    //            #else
    //            { if ( can_draw ) goto do_draw_big; goto do_notseen; }
    //            #endif
    //        #else
    //            if ( pNode->flags & 0x01 )
    //                { if ( can_draw ) goto do_draw_big; goto do_notseen; }
    //        #endif

    //        #if PWR_TILE == 1
    //        //if ( !pNode->idx_children )
    //        {
    //            bool wait;
    //            const SPWTilesCacheSubdiv *subdiv = m_pCache->Subdiv( wait, m_tile, pNode->level, pNode->src_offset );
    //            if ( !subdiv || !subdiv->oct )
    //            {
    //                if ( !wait )
    //                    pNode->flags |= 0x01;
    //                { if ( can_draw ) goto do_draw_big; goto do_notseen; }
    //            }

    //            const int subdiv_count = m_lut_byte_counts.lookup[ subdiv->oct ];
    //            PWUInt32 subdiv_idx = m_nodes.AllocNodes( subdiv_count );
    //            if ( !subdiv_idx )
    //            {
    //                m_notseen.AddWait( subdiv_count );
    //                { if ( can_draw ) goto do_draw_big; goto do_notseen; }
    //            }

    //            pNode->idx_children = subdiv_idx;
    //            pNode->oct = subdiv->oct;
    //            pNode->flags |= 0x01;

    //            SPWRenderNode1<ItemDataSize> *pNewChildren = &m_nodes.Nodes()[ subdiv_idx ];
    //            for ( int i = 0; i < subdiv_count; ++i )
    //            {
    //                SPWRenderNode1<ItemDataSize> *pNewChild = (pNewChildren+i);
    //                //
    //                pNewChild->src_offset = subdiv->offset + i;
    //                pNewChild->idx_children = 0;
    //                //
    //                pNewChild->oct = 0;
    //                pNewChild->level = pNode->level + 1;
    //                pNewChild->flags = 0;
    //                pNewChild->seen = 0;
    //                //
    //                memcpy( pNewChild->data, (const PWUInt8 *)subdiv->datas + i * ItemDataSize, ItemDataSize );
    //            }

    //            node_oct = pNode->oct;
    //        }
    //        #endif
    //    }

    //    const PWUInt32 dir_m8         = ((item.px >> (31-3)) & 8) | ((item.py >> (30-3)) & 16) | ((item.pz >> (29-3)) & 32);
    //    const PWUInt32 idx_base       = ( node_oct << 6 ) | dir_m8;
    //    const int      children_count = m_lut_byte_counts.lookup[ node_oct ];
    //    const PWUInt32 idx_child      = pNode->idx_children;
    //    const int     *orders         = &m_lut_oct_orders.lookup[ idx_base ];

    //    //=========================================================================
    //    //=========================================================================
    //    #ifdef PWR_OPTS
    //    #if PWR_CLIPZ == 0 && PWR_CLIPXY == 0 && PWR_ORTHO == 0
    //    if ( flags & PWR_FLAG_ORTHO )
    //    {
    //        m_ortho_dir_m8 = dir_m8;

    //        PWInt32 otx =  PWR_MULDIV( item.tx, m_tr_kx_64, item.tz );
    //        PWInt32 oty = -PWR_MULDIV( item.ty, m_tr_ky_64, item.tz );

    //        #ifndef PWR_OPT_ORTHOSAMEZ
    //        PWInt32 otz = item.tz << 2;
    //        #endif

    //        int ofs = (((int)item.level) << 3);

    //        int sx1,sx2,sy1,sy2;
    //        for ( int i = 0; i < 8; ++i )
    //        {
    //            PWInt32 ovx = item.tx + m_tr_delta_x[ ofs ];
    //            PWInt32 ovy = item.ty + m_tr_delta_y[ ofs ];
    //            PWInt32 ovz = item.tz + m_tr_delta_z[ ofs ];
    //            ++ofs;

    //            const int sx =  PWR_MULDIV( ovx, m_tr_kx_64, ovz );
    //            const int sy = -PWR_MULDIV( ovy, m_tr_ky_64, ovz );

    //            m_ortho_vx[i] = sx - otx;
    //            m_ortho_vy[i] = sy - oty;

    //            #ifndef PWR_OPT_ORTHOSAMEZ
    //            m_ortho_vz[i] = (ovz << 2) - otz;
    //            #endif

    //            if ( !i )
    //            {
    //                sx1 = sx2 = sx;
    //                sy1 = sy2 = sy;
    //            }
    //            else
    //            {
    //                if ( sx < sx1 ) sx1 = sx;
    //                if ( sx > sx2 ) sx2 = sx;
    //                if ( sy < sy1 ) sy1 = sy;
    //                if ( sy > sy2 ) sy2 = sy;
    //            }
    //        }

    //        m_ortho_sx = ((sx2 - sx1) << 1);
    //        m_ortho_sy = ((sy2 - sy1) << 1);

    //        otx += m_tr_x0_64;
    //        oty += m_tr_y0_64;

    //        #ifdef PWR_SEENORTHO
    //        m_parents[ ++m_parents_pos ] = pNode;
    //        PWUInt8 pops = item.pops + 1;
    //        #endif

    //        for ( int i = children_count-1; i >= 0; --i )
    //        {
    //            const int order = orders[i];
    //            const int child_ofs = order & 255;
    //            const int oct_idx   = order >> 8;

    //            SStackItem & child = m_stack[ ++m_stack_pos ];

    //            child.idx   = idx_child + child_ofs;
    //            child.level = 1; // ortho shift level

    //            #ifdef PWR_SEENORTHO
    //            child.cmask = ( 1 << oct_idx );
    //            child.pops  = pops; pops = 0;
    //            #endif

    //            #ifdef PWR_TEST
    //            child.tmp_index = oct_idx;
    //            #endif

    //            child.tx = otx + m_ortho_vx[ oct_idx ];
    //            child.ty = oty + m_ortho_vy[ oct_idx ];

    //            #ifndef PWR_OPT_ORTHOSAMEZ
    //            child.tz = otz + m_ortho_vz[ oct_idx ];
    //            #endif
    //        }

    //        #if PWR_TILE == 0
    //        RenderMapNode_0010( children_count );
    //        #else
    //        RenderMapNode_0011( children_count );
    //        #endif

    //        #if defined(PWR_SEEN) && !defined(PWR_SEENORTHO)
    //        goto do_notseen2;
    //        #else
    //        continue;
    //        #endif
    //    }
    //    #endif
    //    #endif
    //    //=========================================================================
    //    //=========================================================================

    //    #ifdef PWR_SEEN
    //    m_parents[ ++m_parents_pos ] = pNode;
    //    PWUInt8 pops = item.pops + 1;
    //    #endif

    //    for ( int i = children_count-1; i >= 0; --i )
    //    {
    //        const int order = orders[i];
    //        const int child_ofs = order & 255;
    //        const int oct_idx   = order >> 8;

    //        SStackItem & child = m_stack[ ++m_stack_pos ];

    //        child.idx   = idx_child + child_ofs;
    //        child.level = item.level + 1;

    //        #ifdef PWR_SEEN
    //        child.cmask = ( 1 << oct_idx );
    //        child.pops  = pops; pops = 0;
    //        #endif

    //        #ifdef PWR_TEST
    //        child.tmp_index = oct_idx;
    //        #endif

    //        PWInt32 half = m_tr_qsz >> item.level;
    //        child.px = item.px + ((oct_idx & 1) ? half : -half );
    //        child.py = item.py + ((oct_idx & 2) ? half : -half );
    //        child.pz = item.pz + ((oct_idx & 4) ? half : -half );
    //        
    //        const int ofs = (((int)item.level) << 3) | oct_idx;
    //        child.tx = item.tx + m_tr_delta_x[ ofs ];
    //        child.ty = item.ty + m_tr_delta_y[ ofs ];
    //        child.tz = item.tz + m_tr_delta_z[ ofs ];
    //    }

    //    #ifdef PWR_OPTS
    //    #if PWR_TILE == 0
    //        #if PWR_CLIPZ == 1 && PWR_CLIPXY == 1 && PWR_ORTHO == 0
    //        if      ( flags & PWR_FLAG_XY    ) RenderMapNode_0000( children_count );
    //        else if ( flags & PWR_FLAG_Z     ) RenderMapNode_0100( children_count );
    //        #elif PWR_CLIPZ == 0 && PWR_CLIPXY == 1 && PWR_ORTHO == 0
    //        if      ( flags & PWR_FLAG_XY    ) RenderMapNode_0000( children_count );
    //        #endif
    //    #else
    //        #if PWR_CLIPZ == 1 && PWR_CLIPXY == 1 && PWR_ORTHO == 0
    //        if      ( flags & PWR_FLAG_XY    ) RenderMapNode_0001( children_count );
    //        else if ( flags & PWR_FLAG_Z     ) RenderMapNode_0101( children_count );
    //        #elif PWR_CLIPZ == 0 && PWR_CLIPXY == 1 && PWR_ORTHO == 0
    //        if      ( flags & PWR_FLAG_XY    ) RenderMapNode_0001( children_count );
    //        #endif
    //    #endif
    //    #endif

    //    continue;

    //#ifndef PWR_NOTILES
    //#if PWR_TILE == 0
    //do_tile_children:
    //    //{ if ( can_draw ) goto do_draw_big; goto do_notseen; }

    //    m_tile = pNode->tile;

    //    if ( !pNode->idx_children )
    //    {
    //        bool wait;
    //        const SPWTilesCacheSubdiv *subdiv = m_pCache->Subdiv( wait, m_tile, 0, 0 );
    //        if ( !subdiv || !subdiv->oct )
    //        {
    //            if ( !wait )
    //                pNode->idx_children = 1; // NON-ZERO STOP MARKER (high 8 bits ~ oct==0)
    //            { if ( can_draw ) goto do_draw_big; goto do_notseen; }
    //        }

    //        const int subdiv_count = m_lut_byte_counts.lookup[ subdiv->oct ];
    //        PWUInt32 subdiv_idx = m_nodes.AllocNodes( subdiv_count );
    //        if ( !subdiv_idx )
    //        {
    //            m_notseen.AddWait( subdiv_count );
    //            { if ( can_draw ) goto do_draw_big; goto do_notseen; }
    //        }

    //        pNode->idx_children = (subdiv_idx & 0x00FFFFFFUL) | (((PWUInt32)subdiv->oct) << 24);

    //        SPWRenderNode1<ItemDataSize> *pNewChildren = &m_nodes.Nodes()[ subdiv_idx ];
    //        for ( int i = 0; i < subdiv_count; ++i )
    //        {
    //            SPWRenderNode1<ItemDataSize> *pNewChild = (pNewChildren+i);
    //            //
    //            pNewChild->src_offset = subdiv->offset + i;
    //            pNewChild->idx_children = 0;
    //            //
    //            pNewChild->oct = 0;
    //            pNewChild->level = 1;
    //            pNewChild->flags = 0;
    //            pNewChild->seen = 0;
    //            //
    //            memcpy( pNewChild->data, (const PWUInt8 *)subdiv->datas + i * ItemDataSize, ItemDataSize );
    //        }
    //    }

    //    const int tilenode_oct = ((pNode->idx_children & 0xFF000000UL) >> 24);
    //    if ( !tilenode_oct )
    //        { if ( can_draw ) goto do_draw_big; goto do_notseen; }

    //    const PWUInt32 tilenode_idx = (pNode->idx_children & 0x00FFFFFFUL);

    //    {
    //        const PWUInt32 dir_m8         = ((item.px >> (31-3)) & 8) | ((item.py >> (30-3)) & 16) | ((item.pz >> (29-3)) & 32);
    //        const PWUInt32 idx_base       = ( tilenode_oct << 6 ) | dir_m8;
    //        const int      children_count = m_lut_byte_counts.lookup[ tilenode_oct ];
    //        const PWUInt32 idx_child      = tilenode_idx;
    //        const int     *orders         = &m_lut_oct_orders.lookup[ idx_base ];

    //        #ifdef PWR_SEEN
    //        m_parents[ ++m_parents_pos ] = pNode;
    //        PWUInt8 pops = item.pops + 1;
    //        #endif

    //        for ( int i = children_count-1; i >= 0; --i )
    //        {
    //            const int order = orders[i];
    //            const int child_ofs = order & 255;
    //            const int oct_idx   = order >> 8;

    //            SStackItem & child = m_stack[ ++m_stack_pos ];

    //            child.idx   = idx_child + child_ofs;
    //            child.level = item.level + 1;

    //            #ifdef PWR_SEEN
    //            child.cmask = ( 1 << oct_idx );
    //            child.pops  = pops; pops = 0;
    //            #endif

    //            #ifdef PWR_TEST
    //            child.tmp_index = oct_idx;
    //            #endif

    //            PWInt32 half = m_tr_qsz >> item.level;
    //            child.px = item.px + ((oct_idx & 1) ? half : -half );
    //            child.py = item.py + ((oct_idx & 2) ? half : -half );
    //            child.pz = item.pz + ((oct_idx & 4) ? half : -half );
    //            
    //            const int ofs = (((int)item.level) << 3) | oct_idx;
    //            child.tx = item.tx + m_tr_delta_x[ ofs ];
    //            child.ty = item.ty + m_tr_delta_y[ ofs ];
    //            child.tz = item.tz + m_tr_delta_z[ ofs ];
    //        }

    //        #ifdef PWR_OPTS
    //            #if PWR_CLIPZ == 1 && PWR_CLIPXY == 1 && PWR_ORTHO == 0
    //            RenderMapNode_1101( children_count );
    //            #elif PWR_CLIPZ == 0 && PWR_CLIPXY == 1 && PWR_ORTHO == 0
    //            RenderMapNode_0101( children_count );
    //            #elif PWR_CLIPZ == 0 && PWR_CLIPXY == 0 && PWR_ORTHO == 0
    //            RenderMapNode_0001( children_count );
    //            #elif PWR_CLIPZ == 0 && PWR_CLIPXY == 0 && PWR_ORTHO == 1
    //            RenderMapNode_0011( children_count );
    //            #endif
    //        #else
    //            RenderMapNode_1101( children_count );
    //        #endif

    //        continue;
    //    }

    //    { if ( can_draw ) goto do_draw_big; goto do_notseen; }
    //#endif
    //#endif

    do_draw_small:
        {
            const int sx = m_tr_x0 + PWR_MULDIV( item.tx, m_tr_kx, tz );
            const int sy = m_tr_y0 - PWR_MULDIV( item.ty, m_tr_ky, tz );

            #if PWR_CLIPXY == 1
            if ( sx < 0 || sy < 0 || sx >= m_w || sy >= m_h )
                continue;
            #endif

            const PWInt32 zval = 0x7FFFFFFF - tz;
            const int offset = PWR_SCROFS( sx, sy );

            if ( m_zbuf[ offset ] >= zval )
                continue;

            const void *src_data = pNode->data;

            #ifdef PWR_TEST
            //src_data = &test_colors[ ((item.px >> (31-3)) & 1) | ((item.py >> (30-3)) & 2) | ((item.pz >> (29-3)) & 4) ];
            src_data = &test_colors[ item.tmp1 ];
            #endif

            void *dest = (PWUInt8 *)m_dbuf + offset * ItemDataSize;

            #ifdef PWR_OPT4B
                *((PWUInt32 *)dest) = *((const PWUInt32 *)src_data);
            #else
                memcpy( dest, src_data, ItemDataSize );
            #endif

            m_zbuf[ offset ] = zval;

            #ifdef PWR_STAT
            ++m_stat_nodes_drawn;
            #endif
        }
        continue;

    do_draw_big:
        {
            const PWInt32 zval = draw_zval;

            PWUInt8 *dest = (PWUInt8 *)m_dbuf + draw_ofs * ItemDataSize;
            const void *src_data = pNode->data;

            #ifdef PWR_TEST
            //src_data = &test_colors[ ((item.px >> (31-3)) & 1) | ((item.py >> (30-3)) & 2) | ((item.pz >> (29-3)) & 4) ];
            src_data = &test_colors[ item.tmp1 ];
            #endif

            #ifdef PWR_OPT4B
            const PWUInt32 srcdata = *(PWUInt32 *)src_data;
            #endif

            int offset = draw_ofs;
            for ( int cy = 0; cy < draw_sh; ++cy )
            {
                int ofs = offset;

                #ifndef PWR_OPT4B
                PWUInt8 *wr = dest;
                #endif

                for ( int cx = 0; cx < draw_sw; ++cx )
                {
                    if ( m_zbuf[ ofs ] < zval )
                    {
                        #ifdef PWR_OPT4B
                            *((PWUInt32 *)dest + ofs) = srcdata;
                        #else
                            memcpy( wr + cx * ItemDataSize, src_data, ItemDataSize );
                        #endif

                        m_zbuf[ ofs ] = zval;
                    }
                    ++ofs;
                }

                offset += m_scr_pitch;

                #ifndef PWR_OPT4B
                dest += m_scr_pitch * ItemDataSize;
                #endif
            }

            #ifdef PWR_STAT
            ++m_stat_nodes_drawn;
            #endif
        }
        continue;

    } // while ( stack )
} // PWR_FUNC

    /*
    do_draw_big:
        #ifdef PWR_SEEN
        m_parents_pos -= item.pops;
        #endif

        PWR_ADDNOTSEENCH

        offset = PWR_ZOFFSET( draw_sx, draw_sy );
        PWUInt8 *dest = PWR_DESTPTR( draw_sx, draw_sy );
        const void *src_data = pNode->data;

        #ifdef PWR_TEST
        src_data = &test_colors[ ((item.px >> (31-3)) & 1) | ((item.py >> (30-3)) & 2) | ((item.pz >> (29-3)) & 4) ];
        //src_data = &test_colors[ item.tmp_index ];
        #endif

        for ( int cy = 0; cy < draw_sh; ++cy )
        {
            int ofs = offset;
            PWUInt8 *wr = dest;

            for ( int cx = 0; cx < draw_sw; ++cx )
            {
                if ( m_zbuf[ ofs ] < zval )
                {
                    memcpy( wr + cx * ItemDataSize, src_data, ItemDataSize );
                    m_zbuf[ ofs ] = zval;
                }
                ++ofs;
            }

            offset += m_zpitchDwords;
            dest += m_dpitchBytes;
        }

        #ifdef PWR_STAT
        ++m_stat_nodes_drawn;
        #endif

        continue;

    do_notseen:
        PWR_ADDNOTSEENCH

    #ifdef PWR_OPTS
    #if PWR_CLIPZ == 0 && PWR_CLIPXY == 0 && PWR_ORTHO == 0
    #if defined(PWR_SEEN) && !defined(PWR_SEENORTHO)
    do_notseen2:
    #endif
    #endif
    #endif

        #ifdef PWR_SEEN
        {
            ((PWR_TNODE *)m_parents[ m_parents_pos ])->seen &= ~item.cmask;
            m_parents_pos -= item.pops;
        }
        #endif

        continue;

    #endif
    }
}
*/



/*
        //=========================================================================
        //=========================================================================
        #if PWR_ORTHO == 1
        {
            #ifdef PWR_STAT
            ++m_stat_nodes_ortho_touched;
            #endif

            SStackItem & ref_item = m_stack[ m_stack_pos ];
            --m_stack_pos;

            const PWInt32 otx = ref_item.tx;
            const PWInt32 oty = ref_item.ty;
            const PWInt32 otz = ref_item.tz;

            #if PWR_TILE == 0
            PWR_TNODE *pNode = m_pMap->GetNode( ref_item.idx );
            #else
            PWR_TNODE *pNode = &m_nodes.Nodes()[ ref_item.idx ];
            #endif

            const int ortho_shift = ref_item.level;

            #ifdef PWR_SEENORTHO
            if ( pNode->seen )
                goto do_ortho_children;
            #endif

            int sw_64 = m_ortho_sx >> ortho_shift;
            int sh_64 = m_ortho_sy >> ortho_shift;

            #ifdef PWR_TRICKS
            if ( sw_64 <= 64 ) //&& sh_64 <= 64 )
            #else
            if ( sw_64 <= 64 && sh_64 <= 64 )
            #endif
            {
                const int sx = (otx >> 6);
                const int sy = (oty >> 6);

                const PWInt32 zval = 0x7FFFFFFF - (otz >> 2);
                const int offset = PWR_ZOFFSET( sx, sy );

                if ( m_zbuf[ offset ] >= zval )
                    goto do_ortho_notseen;

                PWR_ADDNOTSEENCH

                const void *src_data = pNode->data;

                #ifdef PWR_TEST
                src_data = &test_colors[ m_ortho_dir_m8 >> 3 ];
                //src_data = &test_colors[ ref_item.tmp_index ];
                #endif

                void *dest = PWR_DESTPTR( sx, sy );
                memcpy( dest, src_data, ItemDataSize );

                m_zbuf[ offset ] = zval;

                #ifdef PWR_SEENORTHO
                {
                    ((PWR_TNODE *)m_parents[ m_parents_pos ])->seen &= ~ref_item.cmask;
                    m_parents_pos -= ref_item.pops;
                }
                #endif

                #ifdef PWR_STAT
                ++m_stat_nodes_ortho_drawn;
                #endif

                continue;
            }

            sw_64 >>= 1; sw_64 += 32;
            sh_64 >>= 1; sh_64 += 32;
            int sx = (otx - sw_64);
            int sy = (oty - sh_64);
            int sw = (otx + sw_64) - sx;
            int sh = (oty + sh_64) - sy;
            sx >>= 6;
            sy >>= 6;
            sw >>= 6;
            sh >>= 6;

            int zoffset = PWR_ZOFFSET( sx, sy );
            const PWInt32 zval = 0x7FFFFFFF - (otz >> 2);

            #ifdef PWR_TRICKS
            if ( ((sw | sh) & 0x7FFFFFFC) == 0 )
                goto do_ortho_draw_big;
            #else
            if ( sw <= 2 || sh <= 2 )
                goto do_ortho_draw_big;
            #endif

            #ifdef PWR_STAT
            ++m_stat_nodes_ortho_checked;
            #endif

            for ( int cy = 0; cy < sh; ++cy )
            {
                int zofs = zoffset;

                for ( int cx = 0; cx < sw; ++cx )
                {
                    if ( m_zbuf[ zofs ] < zval )
                        goto do_ortho_seen;

                    ++zofs;
                }

                zoffset += m_zpitchDwords;
            }

            goto do_ortho_notseen;

        do_ortho_notseen:
            PWR_ADDNOTSEENCH

            #ifdef PWR_SEENORTHO
            {
                ((PWR_TNODE *)m_parents[ m_parents_pos ])->seen &= ~ref_item.cmask;
                m_parents_pos -= ref_item.pops;
            }
            #endif

            continue;

        do_ortho_seen:
            #ifdef PWR_SEENORTHO
            ((PWR_TNODE *)m_parents[ m_parents_pos ])->seen |= ref_item.cmask;
            #endif

        #ifdef PWR_SEENORTHO
        do_ortho_children:
        #endif

            int node_oct = (int)pNode->oct;
            if ( !node_oct )
            {
                #if PWR_TILE == 0
                    #ifndef PWR_NOTILES
                    goto do_ortho_tile_children;
                    #else
                    goto do_ortho_draw_big;
                    #endif
                #else
                    if ( pNode->flags & 0x01 )
                        goto do_ortho_draw_big;
                #endif

                #if PWR_TILE == 1
                //if ( !pNode->idx_children )
                {
                    bool wait;
                    const SPWTilesCacheSubdiv *subdiv = m_pCache->Subdiv( wait, m_tile, pNode->level, pNode->src_offset );
                    if ( !subdiv || !subdiv->oct )
                    {
                        if ( !wait )
                            pNode->flags |= 0x01;
                        goto do_ortho_draw_big;
                    }

                    const int subdiv_count = m_lut_byte_counts.lookup[ subdiv->oct ];
                    PWUInt32 subdiv_idx = m_nodes.AllocNodes( subdiv_count );
                    if ( !subdiv_idx )
                    {
                        m_notseen.AddWait( subdiv_count );
                        goto do_ortho_draw_big;
                    }

                    pNode->idx_children = subdiv_idx;
                    pNode->oct = subdiv->oct;
                    pNode->flags |= 0x01;

                    SPWRenderNode1<ItemDataSize> *pNewChildren = &m_nodes.Nodes()[ subdiv_idx ];
                    for ( int i = 0; i < subdiv_count; ++i )
                    {
                        SPWRenderNode1<ItemDataSize> *pNewChild = (pNewChildren+i);
                        //
                        pNewChild->src_offset = subdiv->offset + i;
                        pNewChild->idx_children = 0;
                        //
                        pNewChild->oct = 0;
                        pNewChild->level = pNode->level + 1;
                        pNewChild->flags = 0;
                        pNewChild->seen = 0;
                        //
                        memcpy( pNewChild->data, (const PWUInt8 *)subdiv->datas + i * ItemDataSize, ItemDataSize );
                    }

                    node_oct = pNode->oct;
                }
                #endif
            }
            
            const PWUInt32 idx_base       = ( node_oct << 6 ) | m_ortho_dir_m8;
            const int      children_count = m_lut_byte_counts.lookup[ node_oct ];
            const PWUInt32 idx_child      = pNode->idx_children;
            const int     *orders         = &m_lut_oct_orders.lookup[ idx_base ];

            #ifdef PWR_SEENORTHO
            m_parents[ ++m_parents_pos ] = pNode;
            PWUInt8 pops = ref_item.pops + 1;
            #endif

            for ( int i = children_count-1; i >= 0; --i )
            {
                const int order = orders[i];
                const int child_ofs = order & 255;
                const int oct_idx   = order >> 8;

                SStackItem & child = m_stack[ ++m_stack_pos ];

                child.idx   = idx_child + child_ofs;
                child.level = ortho_shift + 1; // ortho shift level

                #ifdef PWR_SEENORTHO
                child.cmask = ( 1 << oct_idx );
                child.pops  = pops; pops = 0;
                #endif

                #ifdef PWR_TEST
                child.tmp_index = oct_idx;
                #endif

                child.tx = otx + ( m_ortho_vx[ oct_idx ] >> ortho_shift );
                child.ty = oty + ( m_ortho_vy[ oct_idx ] >> ortho_shift );

                #ifndef PWR_OPT_ORTHOSAMEZ
                child.tz = otz + ( m_ortho_vz[ oct_idx ] >> ortho_shift );
                #else
                child.tz = otz;
                #endif
            }

            continue;

        #ifndef PWR_NOTILES
        #if PWR_TILE == 0
        do_ortho_tile_children:
            //goto do_ortho_draw_big;

            m_tile = pNode->tile;

            if ( !pNode->idx_children )
            {
                bool wait;
                const SPWTilesCacheSubdiv *subdiv = m_pCache->Subdiv( wait, m_tile, 0, 0 );
                if ( !subdiv || !subdiv->oct )
                {
                    if ( !wait )
                        pNode->idx_children = 1; // NON-ZERO STOP MARKER (high 8 bits ~ oct==0)
                    goto do_ortho_draw_big;
                }

                const int subdiv_count = m_lut_byte_counts.lookup[ subdiv->oct ];
                PWUInt32 subdiv_idx = m_nodes.AllocNodes( subdiv_count );
                if ( !subdiv_idx )
                {
                    m_notseen.AddWait( subdiv_count );
                    goto do_ortho_draw_big;
                }

                pNode->idx_children = (subdiv_idx & 0x00FFFFFFUL) | (((PWUInt32)subdiv->oct) << 24);

                SPWRenderNode1<ItemDataSize> *pNewChildren = &m_nodes.Nodes()[ subdiv_idx ];
                for ( int i = 0; i < subdiv_count; ++i )
                {
                    SPWRenderNode1<ItemDataSize> *pNewChild = (pNewChildren+i);
                    //
                    pNewChild->src_offset = subdiv->offset + i;
                    pNewChild->idx_children = 0;
                    //
                    pNewChild->oct = 0;
                    pNewChild->level = 1;
                    pNewChild->flags = 0;
                    pNewChild->seen = 0;
                    //
                    memcpy( pNewChild->data, (const PWUInt8 *)subdiv->datas + i * ItemDataSize, ItemDataSize );
                }
            }

            const int tilenode_oct = ((pNode->idx_children & 0xFF000000UL) >> 24);
            if ( !tilenode_oct )
                goto do_ortho_draw_big;

            const PWUInt32 tilenode_idx = (pNode->idx_children & 0x00FFFFFFUL);

            {
                const PWUInt32 idx_base       = ( tilenode_oct << 6 ) | m_ortho_dir_m8;
                const int      children_count = m_lut_byte_counts.lookup[ tilenode_oct ];
                const PWUInt32 idx_child      = tilenode_idx;
                const int     *orders         = &m_lut_oct_orders.lookup[ idx_base ];

                #ifdef PWR_SEENORTHO
                m_parents[ ++m_parents_pos ] = pNode;
                PWUInt8 pops = ref_item.pops + 1;
                #endif

                for ( int i = children_count-1; i >= 0; --i )
                {
                    const int order = orders[i];
                    const int child_ofs = order & 255;
                    const int oct_idx   = order >> 8;

                    SStackItem & child = m_stack[ ++m_stack_pos ];

                    child.idx   = idx_child + child_ofs;
                    child.level = ortho_shift + 1; // ortho shift level

                    #ifdef PWR_SEENORTHO
                    child.cmask = ( 1 << oct_idx );
                    child.pops  = pops; pops = 0;
                    #endif

                    #ifdef PWR_TEST
                    child.tmp_index = oct_idx;
                    #endif

                    child.tx = otx + ( m_ortho_vx[ oct_idx ] >> ortho_shift );
                    child.ty = oty + ( m_ortho_vy[ oct_idx ] >> ortho_shift );

                    #ifndef PWR_OPT_ORTHOSAMEZ
                    child.tz = otz + ( m_ortho_vz[ oct_idx ] >> ortho_shift );
                    #else
                    child.tz = otz;
                    #endif
                }

                RenderMapNode_0011( children_count );

                continue;
            }

            goto do_ortho_draw_big;
        #endif
        #endif

        do_ortho_draw_big:
            #ifdef PWR_SEENORTHO
            m_parents_pos -= ref_item.pops;
            #endif

            PWR_ADDNOTSEENCH

            int offset = PWR_ZOFFSET( sx, sy );
            PWUInt8 *dest = PWR_DESTPTR( sx, sy );
            const void *src_data = pNode->data;

            #ifdef PWR_TEST
            src_data = &test_colors[ m_ortho_dir_m8 >> 3 ];
            //src_data = &test_colors[ ref_item.tmp_index ];
            #endif

            for ( int cy = 0; cy < sh; ++cy )
            {
                int ofs = offset;
                PWUInt8 *wr = dest;

                for ( int cx = 0; cx < sw; ++cx )
                {
                    if ( m_zbuf[ ofs ] < zval )
                    {
                        memcpy( wr + cx * ItemDataSize, src_data, ItemDataSize );
                        m_zbuf[ ofs ] = zval;
                    }
                    ++ofs;
                }

                offset += m_zpitchDwords;
                dest += m_dpitchBytes;
            }

            #ifdef PWR_STAT
            ++m_stat_nodes_ortho_drawn;
            #endif

            continue;
        }
        #endif

        //=========================================================================
        //=========================================================================
*/