// pw_file_std.cpp
#include "pw_file.h"

#include <stdio.h>
#pragma warning ( disable: 4996 )

//----------------------------------------------------------------

class PWFileImpl : public IPWFile
{
private:
    FILE *m_fp;
    bool m_canWrite;

public:
    PWFileImpl()
    {
        m_fp = 0;
        m_canWrite = false;
    }

    virtual ~PWFileImpl()
    {
        Close();
    }

    // IPWFile
private:
    virtual bool Open_Read( const char *filename, PWFileOffs *pSize=0 )
    {
        Close();

        if ( !filename || !filename[0] )
            return false;

        m_fp = fopen( filename, "rb" );
        if ( !m_fp )
            return false;

        m_canWrite = false;

        if ( pSize )
        {
            if ( 0 != fseek( m_fp, 0, SEEK_END ) )
            {
                Close();
                return false;
            }

            *pSize = (PWFileOffs)ftell( m_fp );
        }

        if ( 0 != fseek( m_fp, 0, SEEK_SET ) )
        {
            Close();
            return false;
        }

        return true;
    }

    virtual bool Open_Modify( const char *filename, PWFileOffs *pSize=0 )
    {
        Close();

        if ( !filename || !filename[0] )
            return false;

        m_fp = fopen( filename, "r+b" );
        if ( !m_fp )
            return false;

        m_canWrite = true;

        if ( pSize )
        {
            if ( 0 != fseek( m_fp, 0, SEEK_END ) )
            {
                Close();
                return false;
            }

            *pSize = (PWFileOffs)ftell( m_fp );
        }

        if ( 0 != fseek( m_fp, 0, SEEK_SET ) )
        {
            Close();
            return false;
        }

        return true;
    }

    virtual bool Open_Rewrite( const char *filename )
    {
        Close();

        if ( !filename || !filename[0] )
            return false;

        m_fp = fopen( filename, "w+b" );
        if ( !m_fp )
            return false;

        m_canWrite = true;

        return true;
    }

    virtual void Close()
    {
        if ( m_fp )
        {
            fclose(m_fp);

            m_fp = 0;
            m_canWrite = false;
        }
    }

    virtual PWFileOffs Tell()
    {
        if ( !m_fp )
            return -1;

        return (PWFileOffs)ftell( m_fp );
    }

    virtual bool Seek( PWFileOffs pos )
    {
        if ( !m_fp )
            return false;

        return 0 == fseek( m_fp, (long)pos, SEEK_SET );
    }

    virtual bool CanWrite()
    {
        return m_canWrite;
    }

    virtual bool Read( void *dest, int size )
    {
        if ( !m_fp )
            return false;

        if ( !dest || size <= 0 )
            return false;

        return size == fread( dest, 1, size, m_fp );
    }

    virtual bool Write( const void *src, int size )
    {
        if ( !m_fp )
            return false;

        if ( !src || size <= 0 )
            return false;

        return size == fwrite( src, 1, size, m_fp );
    }
};

//----------------------------------------------------------------

PWFile::PWFile() { ptr = new PWFileImpl(); }
PWFile::~PWFile() { delete ptr; ptr = 0; }

//----------------------------------------------------------------
