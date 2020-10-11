
#ifndef _ISAAC_TOKEN_H
#define _ISAAC_TOKEN_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ================================= token.h =================================
    Module for containing tokenized information
    ---------------------------------------------------------------------------
*/

#include "UTIL/ground.h"
#include "TOKEN/token_data.h"

// ---------------- tokenid_t ----------------
// Integer capable of holding any token id
typedef unsigned short tokenid_t;

// ---------------- token_t ----------------
// Structure for an individual token
typedef struct {
    tokenid_t id;
    void *data;
} token_t;

// ---------------- token_string_data_t ----------------
// Structure for TOKEN_STRING
typedef struct {
    char *array;
    length_t length;
} token_string_data_t;

// ---------------- tokenlist_t ----------------
// List of tokens and their sources
typedef struct {
    token_t *tokens;
    length_t length;
    length_t capacity;
    source_t *sources;
} tokenlist_t;

// ---------------- tokenlist_print ----------------
// Prints a tokenlist to the terminal
void tokenlist_print(tokenlist_t *tokenlist, const char *buffer);

// ---------------- token_print_literal_value ----------------
// Prints value of literal value token without a newline
void token_print_literal_value(token_t *token);

// ---------------- tokenlist_free ----------------
// Frees a tokenlist completely
void tokenlist_free(tokenlist_t *tokenlist);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_TOKEN_H
