// pw_file.h
#ifndef pw_fileH
#define pw_fileH

#include "../types/pw_ints.h"

//----------------------------------------------------------------

class IPWFile
{
public:
    virtual ~IPWFile() {}

    virtual bool Open_Read( const char *filename, PWFileOffs *pSize=0 ) = 0;
    virtual bool Open_Modify( const char *filename, PWFileOffs *pSize=0 ) = 0;
    virtual bool Open_Rewrite( const char *filename ) = 0;

    virtual void Close() = 0;

    virtual PWFileOffs Tell() = 0;
    virtual bool Seek( PWFileOffs pos ) = 0;

    virtual bool CanWrite() = 0;

    virtual bool Read( void *dest, int size ) = 0;
    virtual bool Write( const void *src, int size ) = 0;
};

//----------------------------------------------------------------

class PWFile
{
public:
   PWFile();
   ~PWFile();

   IPWFile *operator ->() { return ptr; }
   IPWFile *GetPtr() { return ptr; }

private:
    IPWFile *ptr;
};

//----------------------------------------------------------------

#endif
