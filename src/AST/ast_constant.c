
#include <stddef.h>

#include "AST/ast_constant.h"
#include "AST/ast_expr.h"
#include "UTIL/ground.h"
#include "UTIL/string.h"
#include "UTIL/trait.h"

ast_constant_t ast_constant_clone(ast_constant_t *original){
    ast_constant_t clone;
    clone.name = strclone(original->name);
    clone.expression = ast_expr_clone(original->expression);
    clone.traits = original->traits;
    clone.source = original->source;
    return clone;
}

void ast_constant_make_empty(ast_constant_t *out_constant){
    out_constant->name = NULL;
    out_constant->expression = NULL;
    out_constant->traits = TRAIT_NONE;
    out_constant->source = NULL_SOURCE;
}
