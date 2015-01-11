// pw_math.h
#ifndef pw_mathH
#define pw_mathH

#include "../types/pw_ints.h"

//----------------------------------------------------------------

#ifdef _MSC_VER
inline PWInt32 PWMulDiv32( PWInt32 a, PWInt32 b, PWInt32 c )
{
    _asm mov eax, a
    _asm imul b
    _asm idiv c
}
#endif

//----------------------------------------------------------------

#endif
