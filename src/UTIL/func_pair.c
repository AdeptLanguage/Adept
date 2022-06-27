
#include "UTIL/func_pair.h"

void optional_func_pair_set(optional_func_pair_t *result, bool has, func_id_t ast_func_id, func_id_t ir_func_id){
    result->has = has;

    if(has){
        func_pair_t *out_result = &result->value;
        out_result->ast_func_id = ast_func_id;
        out_result->ir_func_id = ir_func_id;
    }
}
