
#include "UTIL/builtin_type.h"

const char * const global_primitives[13] = {
    "bool", "byte", "double", "float", "int", "long", "short",
    "successful", "ubyte", "uint", "ulong", "ushort", "usize"
};

const char * const global_primitives_extended[15] = {
    "bool", "byte", "double", "float", "int", "long", /* Extra */ "ptr",
    "short", "successful", "ubyte", "uint", "ulong", "ushort", "usize", /* Extra */ "void"
};
