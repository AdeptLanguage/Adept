
#include <stdbool.h>
#include <stddef.h>

#include "AST/ast_type.h"
#include "UTIL/func_pair.h"
#include "DRVR/compiler.h"
#include "DRVR/object.h"
#include "IR/ir.h"
#include "IR/ir_value.h"
#include "IRGEN/ir_builder.h"
#include "IRGEN/ir_cache.h"
#include "IRGEN/ir_gen_find.h"
#include "IRGEN/ir_gen_find_sf.h"
#include "UTIL/ground.h"

errorcode_t ir_gen_find_pass_func(ir_builder_t *builder, ir_value_t **argument, ast_type_t *arg_type, optional_func_pair_t *result){
    // Finds the correct __pass__ function for a type
    // NOTE: Returns SUCCESS when a function was found,
    //               FAILURE when a function wasn't found and
    //               ALT_FAILURE when something goes wrong

    ir_gen_sf_cache_entry_t *cache_entry = ir_gen_sf_cache_locate_or_insert(&builder->object->ir_module.sf_cache, arg_type);

    if(ir_gen_sf_cache_read(cache_entry->has_pass, cache_entry->pass, result) == SUCCESS){
        return SUCCESS;
    }

    errorcode_t errorcode = ir_gen_find_func_conforming_without_defaults(builder, "__pass__", argument, arg_type, 1, NULL, true, NULL_SOURCE, result);

    if(errorcode == SUCCESS && result->has){
        cache_entry->pass = result->value;
        cache_entry->has_pass = TROOLEAN_TRUE;
    } else {
        cache_entry->has_pass = TROOLEAN_FALSE;
    }

    return errorcode;
}

errorcode_t ir_gen_find_defer_func(compiler_t *compiler, object_t *object, ast_type_t *arg_type, optional_func_pair_t *result){
    // Finds the correct __defer__ function for a type
    // NOTE: Returns SUCCESS when a function was found,
    //               FAILURE when a function wasn't found and
    //               ALT_FAILURE when something goes wrong

    ir_gen_sf_cache_entry_t *cache_entry = ir_gen_sf_cache_locate_or_insert(&object->ir_module.sf_cache, arg_type);

    if(ir_gen_sf_cache_read(cache_entry->has_defer, cache_entry->defer, result) == SUCCESS){
        return result->has ? SUCCESS : FAILURE;
    }

    // Create temporary AST pointer type without allocating on the heap
    // Will be used as AST type for subject of method during lookup
    ast_type_t ast_type_ptr;
    ast_elem_t *ast_type_ptr_elems[2];
    ast_elem_t ast_type_ptr_elem;
    ast_type_ptr_elem.id = AST_ELEM_POINTER;
    ast_type_ptr_elem.source = arg_type->source;
    ast_type_ptr_elems[0] = &ast_type_ptr_elem;
    ast_type_ptr_elems[1] = arg_type->elements[0];
    ast_type_ptr.elements = ast_type_ptr_elems;
    ast_type_ptr.elements_length = 2;
    ast_type_ptr.source = arg_type->source;

    weak_cstr_t struct_name = ast_type_struct_name(arg_type);
    errorcode_t errorcode;

    if(struct_name){
        errorcode = ir_gen_find_method(compiler, object, struct_name, "__defer__", &ast_type_ptr, 1, NULL_SOURCE, result);
    } else {
        errorcode = FAILURE;
    }

    if(errorcode == SUCCESS && result->has){
        cache_entry->defer = result->value;
        cache_entry->has_defer = TROOLEAN_TRUE;
    } else {
        cache_entry->has_defer = TROOLEAN_FALSE;
    }

    return errorcode;
}

errorcode_t ir_gen_find_assign_func(compiler_t *compiler, object_t *object, ast_type_t *arg_type, optional_func_pair_t *result){
    // Finds the correct __assign__ function for a type
    // NOTE: Returns SUCCESS when a function was found,
    //               FAILURE when a function wasn't found and
    //               ALT_FAILURE when something goes wrong

    ir_gen_sf_cache_entry_t *cache_entry = ir_gen_sf_cache_locate_or_insert(&object->ir_module.sf_cache, arg_type);

    if(ir_gen_sf_cache_read(cache_entry->has_assign, cache_entry->assign, result) == SUCCESS){
        return result->has ? SUCCESS : FAILURE;
    }

    // Create temporary AST pointer type without allocating on the heap
    // Will be used as AST type for subject of method during lookup
    ast_type_t ast_type_ptr;
    ast_elem_t *ast_type_ptr_elems[2];
    ast_elem_t ast_type_ptr_elem;
    ast_type_ptr_elem.id = AST_ELEM_POINTER;
    ast_type_ptr_elem.source = arg_type->source;
    ast_type_ptr_elems[0] = &ast_type_ptr_elem;
    ast_type_ptr_elems[1] = arg_type->elements[0];
    ast_type_ptr.elements = ast_type_ptr_elems;
    ast_type_ptr.elements_length = 2;
    ast_type_ptr.source = arg_type->source;

    ast_type_t args[2];
    args[0] = ast_type_ptr;
    args[1] = *arg_type;

    weak_cstr_t struct_name = ast_type_struct_name(arg_type);
    errorcode_t errorcode;
    
    if(struct_name){
        errorcode = ir_gen_find_method(compiler, object, struct_name, "__assign__", args, 2, NULL_SOURCE, result);
    } else {
        errorcode = FAILURE;
    }

    if(errorcode == SUCCESS && result->has){
        cache_entry->assign = result->value;
        cache_entry->has_assign = TROOLEAN_TRUE;
    } else {
        cache_entry->has_assign = TROOLEAN_FALSE;
    }

    return errorcode;
}
