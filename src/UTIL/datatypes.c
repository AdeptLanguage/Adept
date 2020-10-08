
#include "UTIL/color.h"
#include "UTIL/trait.h"
#include "UTIL/ground.h"
#include "UTIL/datatypes.h"

char *int_to_string_impl(int64 value, char *out_buffer, int base, weak_cstr_t suffix){
    // -----------------------------------------------------------
    // NOTE: For explicit definition, don't rely on itoa/sprintf etc.
    // -----------------------------------------------------------

    // NOTE: Assumes that caller as allocated enough space in 'out_buffer' for the given value
    // RETURNS: 'out_buffer'

    if(value >= 0) return uint_to_string_impl((uint64) value, out_buffer, base, suffix);

    out_buffer[0] = '-';
    uint_to_string_impl((uint64)(-value), &out_buffer[1], base, suffix);
    return out_buffer;
}

char *uint_to_string_impl(uint64 value, char *out_buffer, int base, weak_cstr_t suffix){
    // -----------------------------------------------------------
    // NOTE: For explicit definition, don't rely on itoa/sprintf etc.
    // -----------------------------------------------------------

    // NOTE: Assumes that caller as allocated enough space in 'out_buffer' for the given value
    // RETURNS: 'out_buffer'

    if(value == 0){
        out_buffer[0] = '0';
        memcpy(&out_buffer[1], suffix, strlen(suffix) + 1);
        return out_buffer;
    }

    length_t size = 0;

    while(value != 0){
        out_buffer[size++] = "0123456789ABCDEF"[value % base];
        value /= base;
    }

    // Reverse output
    for(length_t i = 0; i + 1 < size; i++){
        char tmp = out_buffer[size - i - 1];
        out_buffer[size - i - 1] = out_buffer[i];
        out_buffer[i] = tmp;
    }

    memcpy(&out_buffer[size], suffix, strlen(suffix) + 1);
    return out_buffer;
}

strong_cstr_t float64_to_string(float64 value, weak_cstr_t suffix){
    // -----------------------------------------------------------
    // NOTE: For explicit definition, don't rely on sprintf
    // -----------------------------------------------------------

    // TODO: Do a real implementation
    char *memory = (char*) malloc(325 + strlen(suffix));
    sprintf(memory, "%f%s", value, suffix);
    return memory;
}

int64 string_to_int64(weak_cstr_t string, int base){
    // -----------------------------------------------------------
    // NOTE: For explicit definition, don't rely on strtoll
    // -----------------------------------------------------------

    return string[0] == '-' ? -((int64) string_to_uint64(&string[1], base)) : (int64) string_to_uint64(string, base);
}

uint64 string_to_uint64(weak_cstr_t string, int base){
    // -----------------------------------------------------------
    // NOTE: For explicit definition, don't rely on strtoull
    // -----------------------------------------------------------

    length_t length = strlen(string);
    if(length == 0) return 0; // Avoid problems with zero length strings

    uint64 power = 1;
    length_t i = length - 1;
    uint64 result = 0;

    if(base == 10){
        // Faster method for parsing base 10 integers
    
        do {
            char c = string[i];
            if(c < '0' || c > '9') return 0;
            result += (uint64) (c - '0') * power;
            power *= 10;
        } while(i-- != 0);

        return result;
    }

    // Slightly slower method for parsing integers of any base
    // NOTE: Maximum base supported is 36
    
    do {
        char c = string[i];

        // bool is_arabic_digit             = c_traits & TRAIT_1
        // bool is_extended_digit           = c_traits & TRAIT_2
        // bool is_extended_lowercase_digit = c_traits & TRAIT_3
        // bool is_digit                    = c_traits != TRAIT_NONE
        trait_t cinfo = (c >= '0' && c <= '9')
            ? TRAIT_1 // is_arabic_digit
            : (c >= 'A' && c < 'A' + base - 10) // NOTE: Checking that (base > 10) isn't necessary
                ? TRAIT_2 // is_extended_digit
                : (c >= 'a' && c < 'a' + base - 10) // NOTE: Checking that (base > 10) isn't necessary
                    ? TRAIT_3 // is_extended_lowercase_digit
                    : TRAIT_NONE; // (not a digit)
        
        // Return zero if character is invalid digit
        if(cinfo == TRAIT_NONE) return 0;
        
        result += (uint64) (cinfo & TRAIT_1 ? c - '0' : cinfo & TRAIT_2 ? c - 'A' + 10 : c - 'a' + 10) * power;
        power *= base;
    } while(i-- != 0);

    return result;
}

bool string_to_int_must_be_uint64(weak_cstr_t string, length_t length, int base){
    // NOTE: Doesn't care whether 'string' is valid integer representation

    // Base 10
    if(base == 10) return length != 19 ? length > 19 : strncmp(string, "9223372036854775807", 19) > 0;
    
    // Base 16
    if(base == 16) return length != 16 ? length > 16 : strncmp(string, "7FFFFFFFFFFFFFFF", 16) > 0;
    
    // Base Unimplemented
    internalerrorprintf("string_to_int_must_be_uint64() got unimplemented base %d\n", base);
    return false;
}
