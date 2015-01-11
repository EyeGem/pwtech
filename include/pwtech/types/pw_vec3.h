// pw_vec3.h
#ifndef pw_vec3H
#define pw_vec3H

#include <math.h>

//----------------------------------------------------------------

class PWVec3
{
public:
    float x, y, z;

public:
    enum EUninitialized { UNINITIALIZED };

    PWVec3() { Set(0,0,0); }
    PWVec3( EUninitialized ) {}
    PWVec3( float _x, float _y, float _z ) { Set(_x,_y,_z); }

public:
    void operator += ( const PWVec3 & t ) { x += t.x; y += t.y; z += t.z; }
    void operator -= ( const PWVec3 & t ) { x -= t.x; y -= t.y; z -= t.z; }

    void operator *= ( const float k ) { x *= k; y *= k; z *= k; }

public:
    void Set( float _x, float _y, float _z ) { x = _x; y = _y; z = _z; }

    void SetInv( const PWVec3 & t ) { x = -t.x; y = -t.y; z = -t.z; }

    void SetDelta( const PWVec3 & a, const PWVec3 & b )
    {
        x = b.x - a.x;
        y = b.y - a.y;
        z = b.z - a.z;
    }

    void SetLerp( const PWVec3 & a, const PWVec3 & b, float k )
    {
        float ik = 1.0f - k;
        x = a.x * ik + b.x * k;
        y = a.y * ik + b.y * k;
        z = a.z * ik + b.z * k;
    }

    void SetCross( const PWVec3 & a, const PWVec3 & b )
    {
        x = a.y * b.z - a.z * b.y;
        y = a.z * b.x - a.x * b.z;
        z = a.x * b.y - a.y * b.x;
    }

    void SetCross( const PWVec3 & a1, const PWVec3 & a2, const PWVec3 & b1, const PWVec3 & b2 )
    {
        PWVec3 a = a2; a -= a1;
        PWVec3 b = b2; b -= b1;
        SetCross( a, b );
    }

    void Add( const PWVec3 & t, float k )
    {
        x += t.x * k;
        y += t.y * k;
        z += t.z * k;
    }

    void Sub( const PWVec3 & t, float k )
    {
        x -= t.x * k;
        y -= t.y * k;
        z -= t.z * k;
    }

    float Dot( const PWVec3 & t )
    {
        return x * t.x + y * t.y + z * t.z;
    }

    float LengthSq() const
    {
        return x * x + y * y + z * z;
    }

    float Length() const
    {
        return sqrtf( LengthSq() );
    }

    void Normalize( float length = 1.0f )
    {
        float len = Length();
        if ( len <= 0.00001f )
        {
            Set(0,0,0);
            return;
        }

        float k = length / len;
        x *= k;
        y *= k;
        z *= k;
    }
};

//----------------------------------------------------------------

#endif
