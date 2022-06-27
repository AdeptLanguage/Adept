
#include <stdbool.h>
#include <string.h>

#include "IR/ir_func_endpoint.h"
#include "UTIL/ground.h"
#include "UTIL/list.h"
#include "UTIL/util.h"

static bool ir_func_endpoint_is_polymorphic(const ir_func_endpoint_t *endpoint){
    return endpoint->ir_func_id == INVALID_FUNC_ID;
}

static int compare_ir_func_endpoint(const void *raw_a, const void *raw_b){
    const ir_func_endpoint_t *a = raw_a;
    const ir_func_endpoint_t *b = raw_b;

    if(ir_func_endpoint_is_polymorphic(a) != ir_func_endpoint_is_polymorphic(b)){
        // Prefer non-polymorphic functions before polymorphic ones
        return ir_func_endpoint_is_polymorphic(a) ? 1 : -1;
    }

    if(a->ast_func_id != b->ast_func_id){
        // Prefer functions in the order they were defined
        return a->ast_func_id < b->ast_func_id ? -1 : 1;
    }

    return 0;
}

void ir_func_endpoint_list_insert(ir_func_endpoint_list_t *endpoint_list, ir_func_endpoint_t endpoint){
    length_t length = endpoint_list->length;
    length_t position = find_insert_position(endpoint_list->endpoints, length, &compare_ir_func_endpoint, &endpoint, sizeof endpoint);

    list_append_new(endpoint_list, ir_func_endpoint_t);

    memmove(&endpoint_list->endpoints[position + 1], &endpoint_list->endpoints[position], sizeof(ir_func_endpoint_t) * (length - position));
    endpoint_list->endpoints[position] = endpoint;
}
