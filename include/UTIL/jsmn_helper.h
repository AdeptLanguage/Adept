
#ifndef _ISAAC_JSMN_HELPER_H
#define _ISAAC_JSMN_HELPER_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ============================== jsmn_helper.h ==============================
    Module for helper functions for working with jsmn.h
    ---------------------------------------------------------------------------
*/


// ---------------- JSMN_HELPER_LOG_ERRORS ----------------
// When defined, JSMN helper functions will print errors to stdout
#define JSMN_HELPER_LOG_ERRORS
// --------------------------------------------------------

#define JSMN_HEADER
#include "UTIL/jsmn.h"
#include "UTIL/ground.h"

// ---------------- jsmn_helper_print_token ----------------
// Prints details information about a JSMN token
void jsmn_helper_print_token(weak_cstr_t buffer, jsmntok_t token);

// ---------------- jsmn_helper_print_tokens ----------------
// Prints a list of JSMN tokens
void jsmn_helper_print_tokens(jsmntok_t *tokens, length_t length);

// ---------------- jsmn_helper_get_object ----------------
// Returns whether the token at 'index' is a JSMN_OBJECT token
// NOTE: When successful, the number of entries is equal to "tokens[index].size"
successful_t jsmn_helper_get_object(weak_cstr_t buffer, jsmntok_t *tokens, length_t num_tokens, length_t index);

// ---------------- jsmn_helper_get_string ----------------
// Returns whether the token at 'index' is a JSMN_STRING token
// NOTE: Writes raw contents of string to 'out_content' if successful
// NOTE: Will truncate any content longer than 'max_output_size'
// NOTE: max_output_size includes NULL terminating byte
successful_t jsmn_helper_get_string(weak_cstr_t buffer, jsmntok_t *tokens, length_t num_tokens, length_t index, char *out_content, length_t max_output_size);

// ---------------- jsmn_helper_get_integer ----------------
// Returns whether the token at 'index' is a JSMN_PRIMITIVE token
//       and is number-like
// When successful, writes the integer value to 'out_value'
// NOTE: Maximum integer length is 127 bytes, everything after
//       that will be truncated!
successful_t jsmn_helper_get_integer(weak_cstr_t buffer, jsmntok_t *tokens, length_t num_tokens, length_t index, long long *out_value);

// ---------------- jsmn_helper_get_boolean ----------------
// Returns whether the token at 'index' is a JSMN_PRIMITIVE token
//       and is boolean-like
// When successful, writes the boolean value to 'out_value'
successful_t jsmn_helper_get_boolean(weak_cstr_t buffer, jsmntok_t *tokens, length_t num_tokens, length_t index, bool *out_bool);

// ---------------- jsmn_helper_subtoken_count ----------------
// Given a JSMN_OBJECT, JSMN_ARRAY, or JSMN_STRING key token,
// returns the number of tokens that are taken up by that structure
// including its children
// 
// For example {"Hello": "World"} is broken down into tokens
// [JSMN_OBJECT, JSMN_STRING, JSMN_STRING]. If the index
// for the JSMN_OBJECT was specified (aka 0), this function would tell
// you that the JSMN_OBJECT token and its children take up 3 total tokens
length_t jsmn_helper_subtoken_count(jsmntok_t *tokens, length_t start_index);

// ---------------- jsmn_helper_parse_fail_reason ----------------
// Returns a string detailing the cause of parse failure
weak_cstr_t jsmn_helper_parse_fail_reason(int code);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_JSMN_HELPER_H
