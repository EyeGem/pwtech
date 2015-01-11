// pw_file_win.cpp
#include "pw_file.h"

#include <windows.h>

//----------------------------------------------------------------

class PWFileImpl : public IPWFile
{
private:
    HANDLE m_handle;
    bool m_canWrite;

private:
    bool IsOpen()
    {
        return m_handle != INVALID_HANDLE_VALUE;
    }

public:
    PWFileImpl()
    {
        m_handle = INVALID_HANDLE_VALUE;
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

        m_handle = CreateFileA( filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
        if ( !IsOpen() )
            return false;

        m_canWrite = false;

        if ( pSize )
        {
            DWORD high;
            DWORD size = GetFileSize( m_handle, &high );
            if ( size == INVALID_FILE_SIZE )
            {
                Close();
                return false;
            }

            PWFileOffs fullsize;
            fullsize = high;
            fullsize <<= 32;
            fullsize |= size;

            *pSize = fullsize;
        }

        if ( INVALID_SET_FILE_POINTER == SetFilePointer( m_handle, 0, NULL, FILE_BEGIN ) )
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

        m_handle = CreateFileA( filename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
        if ( !IsOpen() )
            return false;

        m_canWrite = true;

        if ( pSize )
        {
            DWORD high;
            DWORD size = GetFileSize( m_handle, &high );
            if ( size == INVALID_FILE_SIZE )
            {
                Close();
                return false;
            }

            PWFileOffs fullsize;
            fullsize = high;
            fullsize <<= 32;
            fullsize |= size;

            *pSize = fullsize;
        }

        if ( INVALID_SET_FILE_POINTER == SetFilePointer( m_handle, 0, NULL, FILE_BEGIN ) )
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

        m_handle = CreateFileA( filename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
        if ( !IsOpen() )
            return false;

        m_canWrite = true;

        return true;
    }

    virtual void Close()
    {
        if ( IsOpen() )
        {
            CloseHandle( m_handle );

            m_handle = INVALID_HANDLE_VALUE;
            m_canWrite = false;
        }
    }

    virtual PWFileOffs Tell()
    {
        if ( !IsOpen() )
            return -1;

        LONG high;
        DWORD pos = SetFilePointer( m_handle, 0, &high, FILE_CURRENT );
        if ( pos == INVALID_SET_FILE_POINTER )
            return -1;

        PWFileOffs fullpos;
        fullpos = high;
        fullpos <<= 32;
        fullpos |= pos;

        return fullpos;
    }

    virtual bool Seek( PWFileOffs pos )
    {
        if ( !IsOpen() )
            return false;

        LONG high = (LONG)(pos >> 32);
        LONG low  = (LONG)(pos & 0xFFFFFFFF);

        return INVALID_SET_FILE_POINTER != SetFilePointer( m_handle, low, &high, FILE_BEGIN );
    }

    virtual bool CanWrite()
    {
        return m_canWrite;
    }

    virtual bool Read( void *dest, int size )
    {
        if ( !IsOpen() )
            return false;

        if ( !dest || size <= 0 )
            return false;

        DWORD outsiz = 0;
        if ( TRUE != ReadFile( m_handle, dest, size, &outsiz, NULL ) )
            return false;

        if ( size != outsiz )
            return false;

        return true;
    }

    virtual bool Write( const void *src, int size )
    {
        if ( !IsOpen() )
            return false;

        if ( !src || size <= 0 )
            return false;

        DWORD outsiz = 0;
        if ( TRUE != WriteFile( m_handle, src, size, &outsiz, NULL ) )
            return false;

        if ( size != outsiz )
            return false;

        return true;
    }
};

//----------------------------------------------------------------

PWFile::PWFile() { ptr = new PWFileImpl(); }
PWFile::~PWFile() { delete ptr; ptr = 0; }

//----------------------------------------------------------------
