
#ifndef _ISAAC_DATATYPES_H
#define _ISAAC_DATATYPES_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ================================ datatypes.h ===============================
    Module containing specific data types
    ----------------------------------------------------------------------------
*/

#include <inttypes.h>
#include "UTIL/ground.h"

typedef int8_t   int8;
typedef int16_t  int16;
typedef int64_t  int64;
typedef int32_t  int32;

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float  float32; // Assume that float  is 32 bit IEEE 754 single-precision
typedef double float64; // Assume that double is 64 bit IEEE 754 double-precision

typedef bool    adept_bool;
typedef uint8   adept_successful;
typedef int8    adept_byte;
typedef int16   adept_short;
typedef int32   adept_int;
typedef int64   adept_long;
typedef uint8   adept_ubyte;
typedef uint16  adept_ushort;
typedef uint32  adept_uint;
typedef uint64  adept_ulong;
typedef float32 adept_float;
typedef float64 adept_double;
typedef uint64  adept_ptr;
typedef uint64  adept_usize;

typedef int64   adept_generic_int;
typedef float64 adept_generic_float;

// ---------------- intXX_to_string ----------------
// Returns a 'strong_cstr_t' containing the signed
// integer value as a c-string in base 10
// NOTE: 'suffix' must be valid
#define int8_to_string(value, suffix)  int_to_string_impl(value, (char*) malloc(5 + strlen(suffix)),  10, suffix)  // "-128\0"
#define int16_to_string(value, suffix) int_to_string_impl(value, (char*) malloc(7 + strlen(suffix)),  10, suffix); // "-32769\0"
#define int32_to_string(value, suffix) int_to_string_impl(value, (char*) malloc(12 + strlen(suffix)), 10, suffix); // "-2147483649\0"
#define int64_to_string(value, suffix) int_to_string_impl(value, (char*) malloc(21 + strlen(suffix)), 10, suffix); // "-9223372036854775808\0"

// ---------------- uintXX_to_string ----------------
// Returns a 'strong_cstr_t' containing the unsigned
// integer value as a c-string in base 10
// NOTE: 'suffix' must be valid
#define uint8_to_string(value, suffix)  uint_to_string_impl(value, (char*) malloc(4 + strlen(suffix)),  10, suffix); // "255\0"
#define uint16_to_string(value, suffix) uint_to_string_impl(value, (char*) malloc(6 + strlen(suffix)),  10, suffix); // "65535\0"
#define uint32_to_string(value, suffix) uint_to_string_impl(value, (char*) malloc(11 + strlen(suffix)), 10, suffix); // "4294967295\0"
#define uint64_to_string(value, suffix) uint_to_string_impl(value, (char*) malloc(21 + strlen(suffix)), 10, suffix); // "18446744073709551615\0"

// ---------------- float32_to_string, float64_to_string ----------------
// Returns a 'strong_cstr_t' containing the floating point value as a c-string in base 10
// NOTE: 'suffix' must be valid
#define float32_to_string(value, suffix) float64_to_string((float64) value, suffix)
strong_cstr_t float64_to_string(float64 value, weak_cstr_t suffix);

// ---------------- string_to_intXX ----------------
// Returns a signed integer value from a string
// NOTE: 'string' must be in the format /[0-9]*/ or /[0-9A-Za-z]*/
// NOTE: 'string' CANNOT be prefexed with '0x', '0o', etc.
// NOTE: 'string' CANNOT be suffixed with 'si', 'sl', etc.
#define string_to_int8(string, base)  ((int8)  string_to_int64(string, base))
#define string_to_int16(string, base) ((int16) string_to_int64(string, base))
#define string_to_int32(string, base) ((int32) string_to_int64(string, base))
int64   string_to_int64(weak_cstr_t string, int base);

// ---------------- string_to_uintXX ----------------
// Returns a unsigned integer value from a string
// NOTE: 'string' must be in the format /[0-9]*/ or /[0-9A-Za-z]*/
// NOTE: 'string' CANNOT be prefexed with '0x', '0o', etc.
// NOTE: 'string' CANNOT be suffixed with 'ui', 'ul', etc.
#define string_to_uint8(string, base)  ((uint8)  string_to_uint64(string, base))
#define string_to_uint16(string, base) ((uint16) string_to_uint64(string, base))
#define string_to_uint32(string, base) ((uint32) string_to_uint64(string, base))
uint64  string_to_uint64(weak_cstr_t string, int base);

// ---------------- string_to_float32, string_to_float64 ----------------
// Returns a floating point value from the given string representation
// Guaranteed to support basic '1.234', '1.23e10', '1.23E10' notation
// NOTE: 'string' CANNOT be suffixed with 'f' or with 'd'
#define string_to_float32(string) ((float32) atof(string)) // 'atof' is good enough for our usage
#define string_to_float64(string) ((float64) atof(string)) // 'atof' is good enough for our usage

// ---------------- int_to_string_impl, uint_to_string_impl ----------------
// NOTE: Assumes that caller as allocated enough space in 'out_buffer' for the given value
// NOTE: 'suffix' must be valid
// NOTE: Maximum 'base' supported is 16
// RETURNS: 'out_buffer'
char  *int_to_string_impl( int64 value, char *out_buffer, int base, weak_cstr_t suffix);
char *uint_to_string_impl(uint64 value, char *out_buffer, int base, weak_cstr_t suffix);

// ---------------- string_to_int_must_be_uint64 ----------------
// Returns whether the only way to represent an integer
// must be to use an unsigned 64-bit integer (assuming 64 bit integer size limit)
// NOTE: Doesn't care whether 'string' is valid integer representation,
//       however the return value means nothing if that's the case
// NOTE: Only supports base 10 and base 16
// NOTE: 'string' must be in the format /[0-9]*/ or /[0-9A-Fa-f]*/
bool string_to_int_must_be_uint64(weak_cstr_t string, length_t length, int base);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_DATATYPES_H
