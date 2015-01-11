// pw_lookups.h
#ifndef pw_lookupsH
#define pw_lookupsH

#include "types/pw_ints.h"

//----------------------------------------------------------------

class PWLUT_ByteCounts
{
public:
    int lookup[ 256 ];

public:
    PWLUT_ByteCounts()
    {
        for ( int i = 0; i < 256; ++i )
        {
            int sum = 0;

            for ( int j = 0; j < 8; ++j )
                if ( i & (1<<j) )
                    ++sum;

            lookup[i] = sum;
        }
    }
};

//----------------------------------------------------------------

class PWLUT_WordCounts
{
public:
    PWUInt8 lookup[ 65536 ];

public:
    PWLUT_WordCounts()
    {
        for ( int i = 0; i < 65536; ++i )
        {
            int sum = 0;

            for ( int j = 0; j < 16; ++j )
                if ( i & (1<<j) )
                    ++sum;

            lookup[i] = (unsigned char)sum;
        }
    }
};

//----------------------------------------------------------------

class PWLUT_OctOffsets
{
public:
    PWUInt8 lookup[ 256 * 8 ];
    //
    // index = ( ((int)oct_byte) << 3 ) | child_idx
    // value = 0..7 or 0xFF (no child for this oct_byte)

public:
    PWLUT_OctOffsets()
    {
        for ( int oct = 0; oct < 256; ++oct )
        {
            PWUInt8 *wr = &lookup[ oct * 8 ];
            
            int ofs = 0;
            for ( int i = 0; i < 8; ++i )
            {
                if ( oct & (1<<i) )
                    wr[i] = ofs++;
                else
                    wr[i] = 0xFF;
            }
        }
    }
};

//----------------------------------------------------------------

class PWLUT_OctIndexes
{
public:
    PWUInt8 lookup[ 256 * 8 ];
    //
    // index = ( ((int)oct_byte) << 3 ) | child_offset(0..7)
    // value = child_idx(0..7) or 0xFF (bad offset)

public:
    PWLUT_OctIndexes()
    {
        for ( int oct = 0; oct < 256; ++oct )
        {
            PWUInt8 *wr = &lookup[ oct * 8 ];
            
            int ofs = 0;
            for ( int i = 0; i < 8; ++i )
            {
                if ( oct & (1<<i) )
                    wr[ofs++] = i;
            }

            while ( ofs < 8 )
                wr[ofs++] = 0xFF;
        }
    }
};

//----------------------------------------------------------------

class PWLUT_OctOrders
{
public:
    int lookup[ 256 * 8 * 8 ];
    //
    // int px,py,pz = vector to node center in camera space (x right, y up, z forward)
    // dir_m8  = ((px >> (31-3)) & 8) | ((py >> (30-3)) & 16) | ((pz >> (29-3)) & 32)
    // idxbase = ( ((int)oct_byte) << 6 ) | dir_m8
    // index   = idxbase | (0..count-1)
    // count   = byte_counts[ oct_byte ]
    // values  = child_offset(0..7) | (child_idx(0..7) << 8)

public:
    PWLUT_OctOrders()
    {
        const int dirs_children[ 8 * 8 ]={
            0, 1,2,4, 3,5,6, 7,
            1, 0,3,5, 2,4,7, 6,
            2, 0,3,6, 1,4,7, 5,
            3, 1,2,7, 0,5,6, 4,
            4, 6,5,0, 7,2,1, 3,
            5, 7,4,1, 6,3,0, 2,
            6, 7,4,2, 5,3,0, 1,
            7, 6,5,3, 4,2,1, 0,
        };

        for ( int oct = 0; oct < 256; ++oct )
        {
            int offsets[8];
            int ofs = 0;
            for ( int child_idx = 0; child_idx < 8; ++child_idx )
            {
                if ( oct & ( 1 << child_idx ) )
                {
                    offsets[ child_idx ] = ofs;
                    ++ofs;
                }
                else
                    offsets[ child_idx ] = 0; // should not be used
            }

            int *order = &lookup[ oct * 64 ];
            for ( int dir = 0; dir < 8; ++dir )
            {
                int pos = 0;
                for ( int i = 0; i < 8; ++i )
                {
                    int child_idx = dirs_children[ dir * 8 + i ];
                    if ( oct & ( 1 << child_idx ) )
                        order[ pos++ ] = offsets[ child_idx ] | (child_idx << 8);
                }

                while ( pos < 8 )
                    order[ pos++ ] = 0; // should not be used

                order += 8;
            }
        }
    }
};

//----------------------------------------------------------------

#endif
