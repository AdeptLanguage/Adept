
#include "DRVR/name_ctx.h"

name_ctx_t NULL_NAME_CTX = (name_ctx_t){NULL, 0, 0};

void name_ctx_init(name_ctx_t *name_ctx){
    name_ctx->namespaces = NULL;
    name_ctx->length = 0;
    name_ctx->capacity = 0;
}

void name_ctx_free(name_ctx_t *name_ctx){
    free(name_ctx->namespaces);
}

void name_ctx_append_used_namespace(name_ctx_t *name_ctx, weak_cstr_t namespace){
    expand((void**) &name_ctx->namespaces, sizeof(weak_lenstr_t), name_ctx->length, &name_ctx->capacity, 1, 4);
    name_ctx->namespaces[name_ctx->length].cstr = namespace;
    name_ctx->namespaces[name_ctx->length++].length = strlen(namespace);
}

void name_ctx_prepend_used_namespace(name_ctx_t *name_ctx, weak_cstr_t namespace){
    expand((void**) &name_ctx->namespaces, sizeof(weak_lenstr_t), name_ctx->length, &name_ctx->capacity, 1, 4);
    memmove(&name_ctx->namespaces[1], &name_ctx->namespaces[0], sizeof(weak_lenstr_t) * (name_ctx->length++));
    name_ctx->namespaces[0].cstr = namespace;
    name_ctx->namespaces[0].length = strlen(namespace);
}

void name_ctx_clone(name_ctx_t *destination, const name_ctx_t *original){
    destination->namespaces = malloc(sizeof(weak_lenstr_t) * original->capacity);
    destination->length = original->length;
    destination->capacity = original->capacity;

    memcpy(destination->namespaces, original->namespaces, original->length);
}
