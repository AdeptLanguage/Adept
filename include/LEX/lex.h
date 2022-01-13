
#ifndef _ISAAC_LEX_H
#define _ISAAC_LEX_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ================================== lex.h ==================================
    Module for performing lexical analysis
    ---------------------------------------------------------------------------
*/

#include <stdbool.h>

#include "DRVR/compiler.h"
#include "DRVR/object.h"
#include "UTIL/ground.h"

// ---------------- lex_state_t ----------------
// Container for lexical analysis state
typedef struct {
    unsigned int state;
    char *buildup;
    length_t buildup_length;
    length_t buildup_capacity;
    length_t buildup_inner_stride;
    bool is_hexadecimal : 1;
    bool can_exp : 1;
    bool can_exp_neg : 1;
} lex_state_t;

// ==============================================================
// -------------- Possible lexical analysis states --------------
// ==============================================================
enum {
    LEX_STATE_IDLE,
    LEX_STATE_WORD,
    LEX_STATE_STRING,
    LEX_STATE_CSTRING,
    LEX_STATE_EQUALS,
    LEX_STATE_NOT,
    LEX_STATE_NUMBER,
    LEX_STATE_CONSTANT,
    LEX_STATE_ADD,
    LEX_STATE_SUBTRACT,
    LEX_STATE_MULTIPLY,
    LEX_STATE_DIVIDE,
    LEX_STATE_MODULUS,
    LEX_STATE_BIT_XOR,
    LEX_STATE_LINECOMMENT,
    LEX_STATE_LONGCOMMENT,
    LEX_STATE_ENDCOMMENT,
    LEX_STATE_LESS,
    LEX_STATE_GREATER,
    LEX_STATE_UBERAND,
    LEX_STATE_UBEROR,
    LEX_STATE_COLON,
    LEX_STATE_META,
    LEX_STATE_POLYMORPH,
    LEX_STATE_COMPLEMENT,
    LEX_STATE_POLYCOUNT,
};

// ---------------- lex ----------------
// Entry point for lexical analysis
errorcode_t lex(compiler_t *compiler, object_t *object);

// ---------------- lex_buffer ----------------
// Lex the text buffer attached to the given object
errorcode_t lex_buffer(compiler_t *compiler, object_t *object);

// ---------------- lex_state_init ----------------
// Initializes lexical analysis state
void lex_state_init(lex_state_t *lex_state);

// ---------------- lex_state_free ----------------
// Frees data within lexical analysis state
void lex_state_free(lex_state_t *lex_state);

// ---------------- lex_get_location ----------------
// Retrieves line and column of an index in a buffer
void lex_get_location(const char *buffer, length_t i, int *line, int *column);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_LEX_H
