
#include <stdbool.h>
#include <stdlib.h>

#include "AST/ast.h"
#include "DRVR/compiler.h"
#include "UTIL/ground.h"

errorcode_t ensure_not_violating_no_discard(compiler_t *compiler, bool no_discard_active, source_t call_source, ast_func_t *callee){
    if(no_discard_active && callee->traits & AST_FUNC_NO_DISCARD){
        strong_cstr_t display = ast_func_head_str(callee);
        compiler_panicf(compiler, call_source, "Not allowed to discard value returned from '%s'", display);
        free(display);
        return ALT_FAILURE;
    }

    return SUCCESS;
}

errorcode_t ensure_not_violating_disallow(compiler_t *compiler, source_t call_source, ast_func_t *callee){
    if(callee->traits & AST_FUNC_DISALLOW){
        strong_cstr_t display = ast_func_head_str(callee);
        compiler_panicf(compiler, call_source, "Cannot use disallowed '%s'", display);
        free(display);
        return ALT_FAILURE;
    }

    return SUCCESS;
}
