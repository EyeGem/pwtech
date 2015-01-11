// pw_buf2d.h
#ifndef pw_buf2dH
#define pw_buf2dH

//----------------------------------------------------------------

template<typename T>
class PWBuf2D
{
public:
    int   width;  // in items
    int   height; // in items
    int   pitch;  // in bytes
    int   size;   // in bytes
    void *data;   // raw memory

private: // NO COPY
    PWBuf2D( const PWBuf2D & );
    void operator = ( const PWBuf2D & );

public:
    PWBuf2D()
    {
        width = 0;
        height = 0;
        pitch = 0;
        data = 0;
    }

    ~PWBuf2D()
    {
        Free();
    }

    void Resize( int w, int h )
    {
        Free();

        if ( w <= 0 || h <= 0 )
            return;

        const int rowsize = w * sizeof(T);
        const int alignsize = 4;

        width  = w;
        height = h;
        pitch  = (((rowsize-1)/alignsize)+1)*alignsize;
        size   = pitch * height;
        data   = new char[ size ];
    }

    void Free()
    {
        if ( data ) { delete[] data; data = 0; }

        width  = 0;
        height = 0;
        pitch  = 0;
        size   = 0;
        data   = 0;
    }
};

//----------------------------------------------------------------

#endif
