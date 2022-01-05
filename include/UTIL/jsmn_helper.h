
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

typedef struct {
    char *content;
    length_t capacity;
} jsmnh_buffer_t;

typedef struct {
    jsmntok_t *tokens;
    length_t length;
} jsmnh_tokens_t;

typedef struct {
    jsmnh_buffer_t fulltext;
    jsmnh_buffer_t value;
    jsmnh_tokens_t tokens;
    jsmntok_t master_token;
    length_t token_index;
    length_t total_sections;
    bool owns_value_buffer : 1;
    bool owns_tokens : 1;
} jsmnh_obj_ctx_t;

// ---------------- jsmnh_obj_ctx_init ----------------
// Initializes a JSMN helper object processing context
// NOTE: Takes ownership of 'tokens' if 'owns_tokens'
// NOTE: Will never take ownership of '*use_existing_value_buffer'
void jsmnh_obj_ctx_init(
    jsmnh_obj_ctx_t *ctx,
    jsmnh_buffer_t fulltext,
    jsmnh_tokens_t tokens,
    jsmntok_t master_token,
    length_t token_index,
    jsmnh_buffer_t *use_existing_value_buffer,
    bool owns_tokens
);

// ---------------- jsmnh_obj_ctx_easy_init ----------------
// Initializes a JSMN helper object processing context
// from a raw text buffer
errorcode_t jsmnh_obj_ctx_easy_init(jsmnh_obj_ctx_t *ctx, char *raw_buffer, length_t raw_buffer_length);

// ---------------- jsmnh_obj_ctx_free ----------------
// Frees a JSMN helper object processing context
void jsmnh_obj_ctx_free(jsmnh_obj_ctx_t *ctx);

// ---------------- jsmnh_obj_ctx_subobject ----------------
// Forks a JSMN helper object processing context
// into another in order to process another object value
// at the current 'ctx->token_index'
errorcode_t jsmnh_obj_ctx_subobject(jsmnh_obj_ctx_t *ctx, jsmnh_obj_ctx_t *forked_output);

// ---------------- jsmnh_obj_ctx_read_key ----------------
// Reads an object key into a context's fixed-size string buffer
// NOTE: Maximum key length is 1024, everything after will be truncated
errorcode_t jsmnh_obj_ctx_read_key(jsmnh_obj_ctx_t *ctx);

// ---------------- jsmnh_obj_ctx_eq ----------------
// Returns whether the fixed-size string buffer
// of a jsmnh_obj_ctx_t is equal to a certain string.
// Can be used to check results of 'jsmnh_obj_ctx_read_key'
bool jsmnh_obj_ctx_eq(jsmnh_obj_ctx_t *ctx, weak_cstr_t string);

// ---------------- jsmnh_obj_ctx_blind_advance ----------------
// Passes over a JSON unit
void jsmnh_obj_ctx_blind_advance(jsmnh_obj_ctx_t *ctx);

// ---------------- jsmnh_print_token ----------------
// Prints details information about a JSMN token
void jsmnh_print_token(weak_cstr_t buffer, jsmntok_t token);

// ---------------- jsmnh_print_tokens ----------------
// Prints a list of JSMN tokens
void jsmnh_print_tokens(jsmntok_t *tokens, length_t length);

// ---------------- jsmnh_get_object ----------------
// Returns whether the token at 'token_index' is a JSMN_OBJECT token
// NOTE: When successful, the number of entries is equal to "tokens[index].size"
successful_t jsmnh_get_object(char *fulltext, jsmnh_tokens_t tokens, length_t token_index);
successful_t jsmnh_obj_ctx_get_object(jsmnh_obj_ctx_t *ctx);

// ---------------- jsmnh_get_array ----------------
// Returns whether the token at 'token_index' is a JSMN_ARRAY token
// NOTE: When successful, the number of entries is equal to "tokens[index].size"
successful_t jsmnh_get_array(char *fulltext, jsmnh_tokens_t tokens, length_t token_index);
successful_t jsmnh_obj_ctx_get_array(jsmnh_obj_ctx_t *ctx);

// ---------------- jsmnh_get_fixed_string ----------------
// Returns whether the token at 'token_index' is a JSMN_STRING token
// NOTE: Writes raw contents of string to 'out_content' if successful
// NOTE: Will truncate any content longer than 'max_output_size'
// NOTE: max_output_size includes NULL terminating byte
successful_t jsmnh_get_fixed_string(char *fulltext, jsmnh_tokens_t tokens, length_t token_index, char *out_content, length_t max_output_size);
successful_t jsmnh_obj_ctx_get_fixed_string(jsmnh_obj_ctx_t *ctx, char *out_content, length_t max_output);

// ---------------- jsmnh_get_variable_string ----------------
// Returns whether the token at 'token_index' is a JSMN_STRING token
// Allocates and copies raw contents of string to 'out_content' if successful'
successful_t jsmnh_get_variable_string(char *fulltext, jsmnh_tokens_t tokens, length_t token_index, strong_cstr_t *out_content);
successful_t jsmnh_obj_ctx_get_variable_string(jsmnh_obj_ctx_t *ctx, strong_cstr_t *out_content);

// ---------------- jsmnh_get_integer ----------------
// Returns whether the token at 'token_index' is a JSMN_PRIMITIVE token
//       and is number-like
// When successful, writes the integer value to 'out_value'
// NOTE: Maximum integer length is 127 bytes, everything after
//       that will be truncated!
successful_t jsmnh_get_integer(char *fulltext, jsmnh_tokens_t tokens, length_t token_index, long long *out_value);
successful_t jsmnh_obj_ctx_get_integer(jsmnh_obj_ctx_t *ctx, long long *out_value);

// ---------------- jsmnh_get_boolean ----------------
// Returns whether the token at 'token_index' is a JSMN_PRIMITIVE token
//       and is boolean-like
// When successful, writes the boolean value to 'out_value'
successful_t jsmnh_get_boolean(char *fulltext, jsmnh_tokens_t tokens, length_t token_index, bool *out_bool);
successful_t jsmnh_obj_ctx_get_boolean(jsmnh_obj_ctx_t *ctx, bool *out_bool);

// ---------------- jsmnh_subtoken_count ----------------
// Given a JSMN_OBJECT, JSMN_ARRAY, or JSMN_STRING key token,
// returns the number of tokens that are taken up by that structure
// including its children
// 
// For example {"Hello": "World"} is broken down into tokens
// [JSMN_OBJECT, JSMN_STRING, JSMN_STRING]. If the index
// for the JSMN_OBJECT was specified (aka 0), this function would tell
// you that the JSMN_OBJECT token and its children take up 3 total tokens
length_t jsmnh_subtoken_count(jsmntok_t *tokens, length_t start_index);

// ---------------- jsmnh_parse_fail_reason ----------------
// Returns a string detailing the cause of parse failure
weak_cstr_t jsmnh_parse_fail_reason(int code);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_JSMN_HELPER_H
