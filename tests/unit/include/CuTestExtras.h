
#ifndef _ISAAC_CUTEST_EXTRAS_H_INCLUDED
#define _ISAAC_CUTEST_EXTRAS_H_INCLUDED

#include "CuTest.h"
#include "UTIL/ground.h"

#define CuAssertIntEquals_Msgf(tc,ms,ex,ac, ...) { \
    strong_cstr_t message_tmp = mallocandsprintf((ms), __VA_ARGS__); \
    CuAssertIntEquals_LineMsg((tc),__FILE__,__LINE__,(message_tmp),(ex),(ac)); \
    free(message_tmp); \
}

#endif // _ISAAC_CUTEST_EXTRAS_H_INCLUDED
