
#ifndef _ISAAC_META_DIRECTIVES_H
#define _ISAAC_META_DIRECTIVES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "UTIL/ground.h"

struct compiler;
struct object;

enum {
    META_EXPR_UNDEF,
    META_EXPR_NULL,
    META_EXPR_TRUE,
    META_EXPR_FALSE,
    META_EXPR_AND,
    META_EXPR_OR,
    META_EXPR_XOR,
    META_EXPR_STR,
    META_EXPR_VAR,
    META_EXPR_INT,
    META_EXPR_FLOAT,
    META_EXPR_ADD,
    META_EXPR_SUB,
    META_EXPR_MUL,
    META_EXPR_DIV,
    META_EXPR_MOD,
    META_EXPR_POW,
    META_EXPR_EQ,
    META_EXPR_NEQ,
    META_EXPR_GT,
    META_EXPR_GTE,
    META_EXPR_LT,
    META_EXPR_LTE,
    META_EXPR_NOT,
};

#define IS_META_EXPR_ID_COLLAPSED(m) ( \
    m == META_EXPR_UNDEF || m == META_EXPR_NULL || m == META_EXPR_TRUE || m == META_EXPR_FALSE || \
    m == META_EXPR_STR   || m == META_EXPR_INT  || m == META_EXPR_FLOAT \
)

typedef struct {
    unsigned int id;
} meta_expr_t;

typedef struct {
    unsigned int id;
    meta_expr_t *a, *b;
} meta_expr_math_t, meta_expr_logical_t, meta_expr_and_t, meta_expr_or_t, meta_expr_xor_t;

typedef struct {
    unsigned int id;
    strong_cstr_t value;
} meta_expr_str_t;

typedef struct {
    unsigned int id;
    strong_cstr_t name;
    source_t source;
} meta_expr_var_t;

typedef struct {
    unsigned int id;
    long long value;
} meta_expr_int_t;

typedef struct {
    unsigned int id;
    double value;
} meta_expr_float_t;

typedef struct {
    unsigned int id;
    meta_expr_t *value;
} meta_expr_not_t;

typedef struct {
    weak_cstr_t name;
    meta_expr_t *value;
} meta_definition_t;

strong_cstr_t meta_expr_str(meta_expr_t *meta);
void meta_expr_free(meta_expr_t *expr);
void meta_expr_free_fully(meta_expr_t *expr);

meta_expr_t *meta_expr_clone(meta_expr_t *expr);

errorcode_t meta_collapse(struct compiler *compiler, struct object *object, meta_definition_t *definitions, length_t definitions_length, meta_expr_t **expr);
errorcode_t meta_expr_into_bool(struct compiler *compiler, struct object *object, meta_definition_t *definitions, length_t definitions_length, meta_expr_t **expr, bool *out);
errorcode_t meta_expr_into_int(struct compiler *compiler, struct object *object, meta_definition_t *definitions, length_t definitions_length, meta_expr_t **expr, long long *out);
errorcode_t meta_expr_into_float(struct compiler *compiler, struct object *object, meta_definition_t *definitions, length_t definitions_length, meta_expr_t **expr, double *out);
errorcode_t meta_expr_into_string(struct compiler *compiler, struct object *object, meta_definition_t *definitions, length_t definitions_length, meta_expr_t **expr, strong_cstr_t *out);

void meta_definition_add(meta_definition_t **definitions, length_t *length, length_t *capacity, weak_cstr_t name, meta_expr_t *value);
void meta_definition_add_bool(meta_definition_t **definitions, length_t *length, length_t *capacity, weak_cstr_t name, bool boolean);
void meta_definition_add_str(meta_definition_t **definitions, length_t *length, length_t *capacity, weak_cstr_t name, strong_cstr_t content);
void meta_definition_add_float(meta_definition_t **definitions, length_t *length, length_t *capacity, weak_cstr_t name, double content);
void meta_definition_add_int(meta_definition_t **definitions, length_t *length, length_t *capacity, weak_cstr_t name, long long content);

meta_definition_t *meta_definition_find(meta_definition_t *definitions, length_t length, weak_cstr_t name);

meta_expr_t *meta_get_special_variable(struct compiler *compiler, struct object *object, weak_cstr_t variable_name, source_t variable_source);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_META_DIRECTIVES_H
