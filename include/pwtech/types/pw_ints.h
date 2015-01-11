// pw_ints.h
#ifndef pw_intsH
#define pw_intsH

//----------------------------------------------------------------

#ifdef _MSC_VER

typedef signed char      PWInt8;
typedef signed short     PWInt16;
typedef signed int       PWInt32;

typedef __int64          PWInt64;

typedef unsigned char    PWUInt8;
typedef unsigned short   PWUInt16;
typedef unsigned int     PWUInt32;

typedef __int64          PWFileOffs;

#endif

//----------------------------------------------------------------

#endif
