
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

// ---------------- lex ----------------
// Entry point for lexical analysis
errorcode_t lex(compiler_t *compiler, object_t *object);

// ---------------- lex_buffer ----------------
// Lex the text buffer attached to the given object
// NOTE: The attached buffer 'object->buffer' must be terminated with '\n\0'
// NOTE: The final \0 is not included in the 'object->buffer_size'
errorcode_t lex_buffer(compiler_t *compiler, object_t *object);

// ---------------- lex_get_location ----------------
// Retrieves line and column of an index in a buffer
void lex_get_location(const char *buffer, length_t i, int *line, int *column);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_LEX_H
