// pw_transform3.h
#ifndef pw_transform3H
#define pw_transform3H

#include "pw_vec3.h"

#include <math.h>

//----------------------------------------------------------------

class Transform3D
{
public:
    PWVec3 i,j,k,t;

public:
    enum ERotateX { ROTATE_X };
    enum ERotateY { ROTATE_Y };
    enum ERotateZ { ROTATE_Z };
    enum EInverseDelta { INVERSE_DELTA };

public:
    Transform3D()
        : i( PWVec3::UNINITIALIZED )
        , j( PWVec3::UNINITIALIZED )
        , k( PWVec3::UNINITIALIZED )
        , t( PWVec3::UNINITIALIZED )
    {
        SetIdentity();
    }

    Transform3D( float scale )
        : i( PWVec3::UNINITIALIZED )
        , j( PWVec3::UNINITIALIZED )
        , k( PWVec3::UNINITIALIZED )
        , t( PWVec3::UNINITIALIZED )
    {
        SetScale( scale, scale, scale );
    }

    Transform3D( float sx, float sy, float sz )
        : i( PWVec3::UNINITIALIZED )
        , j( PWVec3::UNINITIALIZED )
        , k( PWVec3::UNINITIALIZED )
        , t( PWVec3::UNINITIALIZED )
    {
        SetScale( sx, sy, sz );
    }

    Transform3D( ERotateX, float angle )
        : i( PWVec3::UNINITIALIZED )
        , j( PWVec3::UNINITIALIZED )
        , k( PWVec3::UNINITIALIZED )
        , t( PWVec3::UNINITIALIZED )
    {
        SetRotateX( angle );
    }

    Transform3D( ERotateY, float angle )
        : i( PWVec3::UNINITIALIZED )
        , j( PWVec3::UNINITIALIZED )
        , k( PWVec3::UNINITIALIZED )
        , t( PWVec3::UNINITIALIZED )
    {
        SetRotateY( angle );
    }

    Transform3D( ERotateZ, float angle )
        : i( PWVec3::UNINITIALIZED )
        , j( PWVec3::UNINITIALIZED )
        , k( PWVec3::UNINITIALIZED )
        , t( PWVec3::UNINITIALIZED )
    {
        SetRotateZ( angle );
    }

    Transform3D( const PWVec3 & delta )
        : i( PWVec3::UNINITIALIZED )
        , j( PWVec3::UNINITIALIZED )
        , k( PWVec3::UNINITIALIZED )
        , t( PWVec3::UNINITIALIZED )
    {
        SetTranslation( delta );
    }

    Transform3D( EInverseDelta, const PWVec3 & delta )
        : i( PWVec3::UNINITIALIZED )
        , j( PWVec3::UNINITIALIZED )
        , k( PWVec3::UNINITIALIZED )
        , t( PWVec3::UNINITIALIZED )
    {
        SetTranslation_Inv( delta );
    }

    Transform3D( const Transform3D & a, const Transform3D & b )
        : i( PWVec3::UNINITIALIZED )
        , j( PWVec3::UNINITIALIZED )
        , k( PWVec3::UNINITIALIZED )
        , t( PWVec3::UNINITIALIZED )
    {
        SetCombined( a, b );
    }

    Transform3D( const PWVec3 & pos, float yaw, float pitch, float roll, bool inverse )
        : i( PWVec3::UNINITIALIZED )
        , j( PWVec3::UNINITIALIZED )
        , k( PWVec3::UNINITIALIZED )
        , t( PWVec3::UNINITIALIZED )
    {
        if ( inverse )
            SetYPRInverse( pos, yaw, pitch, roll );
        else
            SetYPRForward( pos, yaw, pitch, roll );
    }

public:
    void SetIdentity()
    {
        i.Set( 1, 0, 0 );
        j.Set( 0, 1, 0 );
        k.Set( 0, 0, 1 );
        t.Set( 0, 0, 0 );
    }

    void SetScale( float sx, float sy, float sz )
    {
        i.Set( sx,  0,  0 );
        j.Set(  0, sy,  0 );
        k.Set(  0,  0, sz );
        t.Set(  0,  0,  0 );
    }

    void SetRotateX( float angle )
    {
        const float sn = sinf( angle );
        const float cs = cosf( angle );

        i.Set(   1,   0,   0 );
        j.Set(   0,  cs,  sn );
        k.Set(   0, -sn,  cs );
        t.Set(   0,   0,   0 );
    }

    void SetRotateY( float angle )
    {
        const float sn = sinf( angle );
        const float cs = cosf( angle );

        i.Set(  cs,   0,  sn );
        j.Set(   0,   1,   0 );
        k.Set( -sn,   0,  cs );
        t.Set(   0,   0,   0 );
    }

    void SetRotateZ( float angle )
    {
        const float sn = sinf( angle );
        const float cs = cosf( angle );

        i.Set(  cs,  sn,   0 );
        j.Set( -sn,  cs,   0 );
        k.Set(   0,   0,   1 );
        t.Set(   0,   0,   0 );
    }

    void SetTranslation( const PWVec3 & delta )
    {
        i.Set( 1, 0, 0 );
        j.Set( 0, 1, 0 );
        k.Set( 0, 0, 1 );
        t = delta;
    }

    void SetTranslation_Inv( const PWVec3 & delta )
    {
        i.Set( 1, 0, 0 );
        j.Set( 0, 1, 0 );
        k.Set( 0, 0, 1 );
        t.SetInv( delta );
    }

    void SetCombined( const Transform3D & a, const Transform3D & b )
    {
        i.x = a.i.x * b.i.x + a.i.y * b.j.x + a.i.z * b.k.x;
        i.y = a.i.x * b.i.y + a.i.y * b.j.y + a.i.z * b.k.y;
        i.z = a.i.x * b.i.z + a.i.y * b.j.z + a.i.z * b.k.z;

        j.x = a.j.x * b.i.x + a.j.y * b.j.x + a.j.z * b.k.x;
        j.y = a.j.x * b.i.y + a.j.y * b.j.y + a.j.z * b.k.y;
        j.z = a.j.x * b.i.z + a.j.y * b.j.z + a.j.z * b.k.z;

        k.x = a.k.x * b.i.x + a.k.y * b.j.x + a.k.z * b.k.x;
        k.y = a.k.x * b.i.y + a.k.y * b.j.y + a.k.z * b.k.y;
        k.z = a.k.x * b.i.z + a.k.y * b.j.z + a.k.z * b.k.z;

        t.x = a.t.x * b.i.x + a.t.y * b.j.x + a.t.z * b.k.x + b.t.x;
        t.y = a.t.x * b.i.y + a.t.y * b.j.y + a.t.z * b.k.y + b.t.y;
        t.z = a.t.x * b.i.z + a.t.y * b.j.z + a.t.z * b.k.z + b.t.z;
    }

    void SetYPRInverse( const PWVec3 & pos, float yaw, float pitch, float roll )
    {
        Transform3D tmp1( INVERSE_DELTA, pos );
        Transform3D tmp2( ROTATE_Y, -yaw );
        SetCombined( tmp1, tmp2 );

        tmp1.SetRotateX( -pitch );
        tmp2.SetCombined( *this, tmp1 );

        tmp1.SetRotateZ( -roll );
        SetCombined( tmp2, tmp1 );
    }

    void SetYPRForward( const PWVec3 & pos, float yaw, float pitch, float roll )
    {
        Transform3D tmp1( ROTATE_Z, roll );
        Transform3D tmp2( ROTATE_X, pitch );
        SetCombined( tmp1, tmp2 );

        tmp1.SetRotateY( yaw );
        tmp2.SetCombined( *this, tmp1 );

        tmp1.SetTranslation( pos );
        SetCombined( tmp2, tmp1 );
    }

public:
    void Apply( PWVec3 *vecOut, const PWVec3 *vecIn, int count ) const
    {
        if ( !vecOut || !vecIn )
            return;

        for ( int idx = 0; idx < count; ++idx )
        {
            const float vx = vecIn->x;
            const float vy = vecIn->y;
            const float vz = vecIn->z;
            ++vecIn;

            vecOut->x = vx * i.x + vy * j.x + vz * k.x + t.x;
            vecOut->y = vx * i.y + vy * j.y + vz * k.y + t.y;
            vecOut->z = vx * i.z + vy * j.z + vz * k.z + t.z;
            ++vecOut;
        }
    }

    void Apply( double & x, double & y, double & z ) const
    {
        const double vx = x;
        const double vy = y;
        const double vz = z;

        x = vx * i.x + vy * j.x + vz * k.x + t.x;
        y = vx * i.y + vy * j.y + vz * k.y + t.y;
        z = vx * i.z + vy * j.z + vz * k.z + t.z;
    }

    void ApplyWithScale( PWVec3 *vecOut, const PWVec3 *vecIn, int count, const PWVec3 & scale ) const
    {
        if ( !vecOut || !vecIn )
            return;

        for ( int idx = 0; idx < count; ++idx )
        {
            const float vx = vecIn->x * scale.x;
            const float vy = vecIn->y * scale.y;
            const float vz = vecIn->z * scale.z;
            ++vecIn;

            vecOut->x = vx * i.x + vy * j.x + vz * k.x + t.x;
            vecOut->y = vx * i.y + vy * j.y + vz * k.y + t.y;
            vecOut->z = vx * i.z + vy * j.z + vz * k.z + t.z;
            ++vecOut;
        }
    }

public:
    void ApplyToNormals( PWVec3 *vecOut, const PWVec3 *vecIn, int count ) const
    {
        if ( !vecOut || !vecIn )
            return;

        for ( int idx = 0; idx < count; ++idx )
        {
            const float vx = vecIn->x;
            const float vy = vecIn->y;
            const float vz = vecIn->z;
            ++vecIn;

            vecOut->x = vx * i.x + vy * j.x + vz * k.x;
            vecOut->y = vx * i.y + vy * j.y + vz * k.y;
            vecOut->z = vx * i.z + vy * j.z + vz * k.z;
            ++vecOut;
        }
    }
};

//----------------------------------------------------------------

#endif
