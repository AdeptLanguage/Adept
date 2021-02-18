
#ifndef _ISAAC_NAME_CTX_H
#define _ISAAC_NAME_CTX_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ================================ name_ctx.h ================================
    Module for the name resolution context data structure
    ----------------------------------------------------------------------------
*/

#include "UTIL/ground.h"

extern name_ctx_t NULL_NAME_CTX;

// ------------------ name_ctx_t ------------------
// Context for which a name resides in.
// Consists of an owned array of "used" namespaces
typedef struct {
    weak_lenstr_t *namespaces;
    length_t length;
    length_t capacity;
} name_ctx_t;

// ------------------ name_ctx_init ------------------
// Initializes a 'name_ctx_t'
void name_ctx_init(name_ctx_t *name_ctx);

// ------------------ name_ctx_free ------------------
// Frees a 'name_ctx_t'
void name_ctx_free(name_ctx_t *name_ctx);

// ------------------ name_ctx_append_used_namespace ------------------
// Appends a namespace to the list of "used" namespaces for a name context
void name_ctx_append_used_namespace(name_ctx_t *name_ctx, weak_cstr_t namespace);

// ------------------ name_ctx_prepend_used_namespace ------------------
// Prepend a namespace to the list of "used" namespaces for a name context
void name_ctx_prepend_used_namespace(name_ctx_t *name_ctx, weak_cstr_t namespace);

// ------------------ name_ctx_clone ------------------
// Clones a 'name_ctx_t'
void name_ctx_clone(name_ctx_t *destination, const name_ctx_t *original);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_OBJECT_H
