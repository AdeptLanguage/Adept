
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "AST/ast_type.h"
#include "UTIL/func_pair.h"
#include "DRVR/compiler.h"
#include "DRVR/object.h"
#include "IRGEN/ir_gen_check_prereq.h"
#include "IRGEN/ir_gen_find_sf.h"
#include "UTIL/builtin_type.h"
#include "UTIL/color.h"
#include "UTIL/ground.h"

static int compare_strings(const void *raw_a, const void *raw_b){
    const char *a = *((const char**) raw_a);
    const char *b = *((const char**) raw_b);
    return strcmp(a, b);
}

static errorcode_t ir_gen_check_prereq_assign(compiler_t *compiler, object_t *object, ast_type_t *concrete_type, bool *out_meets){
    optional_func_pair_t result;

    errorcode_t res = ir_gen_find_assign_func(compiler, object, concrete_type, &result);
    if(res == ALT_FAILURE) return res;

    *out_meets = (res == SUCCESS && result.has);
    return SUCCESS;
}

static ast_elem_base_t *try_ast_elem_base(ast_elem_t *elem){
    return elem->id == AST_ELEM_BASE ? (ast_elem_base_t*) elem : NULL;
}

static errorcode_t ir_gen_check_prereq_number(ast_elem_t *concrete_elem, bool *out_meets){
    ast_elem_base_t *base_elem = try_ast_elem_base(concrete_elem);
    *out_meets = base_elem && typename_builtin_type(base_elem->base) != BUILTIN_TYPE_NONE;
    return SUCCESS;
}

static errorcode_t ir_gen_check_prereq_pod(compiler_t *compiler, object_t *object, ast_type_t *concrete_type, bool *out_meets){
    if(ir_gen_check_prereq_assign(compiler, object, concrete_type, out_meets)) return FAILURE;

    // This prerequisite is just the opposite of the '__assign__' prerequisite
    *out_meets = !*out_meets;
    return SUCCESS;
}

static errorcode_t ir_gen_check_prereq_primitive(ast_elem_t *concrete_elem, bool *out_meets){
    ast_elem_base_t *base_elem = try_ast_elem_base(concrete_elem);
    *out_meets = base_elem && typename_is_extended_builtin_type(base_elem->base);
    return SUCCESS;
}

static errorcode_t ir_gen_check_prereq_signed(ast_elem_t *concrete_elem, bool *out_meets){
    ast_elem_base_t *base_elem = try_ast_elem_base(concrete_elem);

    if(base_elem != NULL){
        weak_cstr_t base = base_elem->base;
    
        if(streq(base, "byte") || streq(base, "short") || streq(base, "int") || streq(base, "long")){
            *out_meets = true;
            return SUCCESS;
        }
    }

    *out_meets = false;
    return SUCCESS;
}

static errorcode_t ir_gen_check_prereq_struct(ast_elem_t *concrete_elem, bool *out_meets){
    if(ir_gen_check_prereq_primitive(concrete_elem, out_meets)) return FAILURE;

    // This prerequisite is just the opposite of the '__primitive__' prerequisite
    *out_meets = !*out_meets;
    return SUCCESS;
}

static errorcode_t ir_gen_check_prereq_unsigned(ast_elem_t *concrete_elem, bool *out_meets){
    ast_elem_base_t *base_elem = try_ast_elem_base(concrete_elem);

    if(base_elem != NULL){
        weak_cstr_t base = base_elem->base;

        if(streq(base, "ubyte") || streq(base, "ushort") || streq(base, "uint") || streq(base, "ulong") || streq(base, "usize")){
            *out_meets = true;
            return SUCCESS;
        }
    }

    *out_meets = false;
    return SUCCESS;
}

errorcode_t ir_gen_check_prereq(
    compiler_t *compiler,
    object_t *object,
    ast_type_t *concrete_type_view,
    enum special_prereq special_prereq,
    bool *out_meets
){
    ast_elem_t *concrete_elem = concrete_type_view->elements[0];

    switch(special_prereq){
    case SPECIAL_PREREQ_ASSIGN:
        // $T~__assign__
        return ir_gen_check_prereq_assign(compiler, object, concrete_type_view, out_meets);
    case SPECIAL_PREREQ_NUMBER:
        // $T~__number__
        return ir_gen_check_prereq_number(concrete_elem, out_meets);
    case SPECIAL_PREREQ_POD:
        // $T~__pod__
        return ir_gen_check_prereq_pod(compiler, object, concrete_type_view, out_meets);
    case SPECIAL_PREREQ_PRIMITIVE:
        // $T~__primitive__
        return ir_gen_check_prereq_primitive(concrete_elem, out_meets);
    case SPECIAL_PREREQ_SIGNED:
        // $T~__signed__
        return ir_gen_check_prereq_signed(concrete_elem, out_meets);
    case SPECIAL_PREREQ_STRUCT:
        // $T~__struct__
        return ir_gen_check_prereq_struct(concrete_elem, out_meets);
    case SPECIAL_PREREQ_UNSIGNED:
        // $T~__unsigned__
        return ir_gen_check_prereq_unsigned(concrete_elem, out_meets);
    case NUM_SPECIAL_PREREQ:;
        // Ignore -Wswitch for NUM_SPECIAL_PREREQ
    }

    if(special_prereq < NUM_SPECIAL_PREREQ){
        internalerrorprintf("ir_gen_check_prereq() - Unrecognized special polymorphic prerequisite '%s'\n", global_special_prerequisites[special_prereq]);
    } else {
        internalerrorprintf("ir_gen_check_prereq() - Unrecognized special polymorphic prerequisite with id '%d'\n", special_prereq);
    }

    return FAILURE;
}

bool is_special_prerequisite(maybe_null_weak_cstr_t prerequisite_name, enum special_prereq *out_special_prerequisite){
    if(prerequisite_name == NULL) return false;

    const char **result = bsearch(&prerequisite_name, global_special_prerequisites, NUM_SPECIAL_PREREQ, sizeof(const char*), &compare_strings);

    if(result && out_special_prerequisite != NULL){
        *out_special_prerequisite = ((enum special_prereq)((char*) result - (char*) global_special_prerequisites) / sizeof(const char*));
    }

    return result != NULL;
}
