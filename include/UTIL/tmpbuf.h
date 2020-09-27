
#ifndef _ISAAC_TMPBUF_H
#define _ISAAC_TMPBUF_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ================================= tmpbuf.h =================================
    Module for quick shared memory used for temporary allocations
    ----------------------------------------------------------------------------
*/

#include "UTIL/ground.h"

// ---------------- tmpbuf_t ----------------
// Shared buffer for quick temporary allocation
typedef struct {
    char *buffer;
    length_t capacity;
} tmpbuf_t;

// ---------------- tmpbuf_init ----------------
// Initializes a tmpbuf_t
void tmpbuf_init(tmpbuf_t *tmpbuf);

// ---------------- tmpbuf_free ----------------
// Frees the memory allocated by tmpbuf_t
void tmpbuf_free(tmpbuf_t *tmpbuf);

// ---------------- tmpbuf_quick_concat3 ----------------
// Quick temporary string concatenation for IR function finding
weak_cstr_t tmpbuf_quick_concat3(tmpbuf_t *tmp, const char *a, const char *b, const char *c);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_TMPBUF_H

