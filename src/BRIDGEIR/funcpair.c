
#include "BRIDGEIR/funcpair.h"

void optional_funcpair_set(optional_funcpair_t *result, bool has, func_id_t ast_func_id, func_id_t ir_func_id, object_t *object){
    result->has = has;

    if(has){
        funcpair_t *out_result = &result->value;
        out_result->ast_func_id = ast_func_id;
        out_result->ir_func_id = ir_func_id;
    }
}
