
#ifndef _ISAAC_FUNCPAIR_H
#define _ISAAC_FUNCPAIR_H

#include <stdbool.h>

#include "AST/ast.h"
#include "UTIL/ground.h"
#include "UTIL/list.h"

// ---------------- func_pair_t ----------------
// A container for function results
typedef struct {
    func_id_t ast_func_id;
    func_id_t ir_func_id;
} func_pair_t;

typedef optional(func_pair_t) optional_func_pair_t;
void optional_func_pair_set(optional_func_pair_t *result, bool has, func_id_t ast_func_id, func_id_t ir_func_id);

typedef listof(func_pair_t, pairs) func_pair_list_t;
#define func_pair_list_append(LIST, VALUE) list_append((LIST), (VALUE), func_pair_t)
#define func_pair_list_free(LIST) free((LIST)->pairs)

#endif // _ISAAC_FUNCPAIR_H
