
#ifndef _ISAAC_BULITIN_TYPE_H
#define _ISAAC_BULITIN_TYPE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "UTIL/ground.h"
#include "UTIL/search.h"

#define BUILTIN_TYPE_NONE      -1
#define BUILTIN_TYPE_BOOL       0
#define BUILTIN_TYPE_BYTE       1
#define BUILTIN_TYPE_DOUBLE     2
#define BUILTIN_TYPE_FLOAT      3
#define BUILTIN_TYPE_INT        4
#define BUILTIN_TYPE_LONG       5
#define BUILTIN_TYPE_SHORT      6
#define BUILTIN_TYPE_SUCCESSFUL 7
#define BUILTIN_TYPE_UBYTE      8
#define BUILTIN_TYPE_UINT       9
#define BUILTIN_TYPE_ULONG      10
#define BUILTIN_TYPE_USHORT     11
#define BUILTIN_TYPE_USIZE      12

const char * const global_primitives[13];
const char * const global_primitives_extended[15];

#define typename_builtin_type(base) binary_string_search(global_primitives, 13, base)

#define typename_is_entended_builtin_type(base) (binary_string_search(global_primitives_extended, 15, base) != -1)

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_BULITIN_TYPE_H