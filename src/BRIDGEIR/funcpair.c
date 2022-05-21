
#include "BRIDGEIR/funcpair.h"

void optional_funcpair_set(optional_funcpair_t *result, bool has, funcid_t ast_func_id, funcid_t ir_func_id, object_t *object){
    result->has = has;

    if(has){
        funcpair_t *out_result = &result->value;
        out_result->ast_func = &object->ast.funcs[ast_func_id];
        out_result->ir_func = &object->ir_module.funcs.funcs[ir_func_id];
        out_result->ast_func_id = ast_func_id;
        out_result->ir_func_id = ir_func_id;
    }
}
