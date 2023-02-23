
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AST/POLY/ast_resolve.h"
#include "AST/TYPE/ast_type_identical.h"
#include "AST/ast.h"
#include "AST/ast_expr.h"
#include "AST/ast_layout.h"
#include "AST/ast_poly_catalog.h"
#include "AST/ast_type.h"
#include "BRIDGEIR/rtti.h"
#include "DRVR/compiler.h"
#include "DRVR/object.h"
#include "IR/ir.h"
#include "IR/ir_pool.h"
#include "IR/ir_type.h"
#include "IR/ir_type_map.h"
#include "IR/ir_value.h"
#include "IRGEN/ir_build_instr.h"
#include "IRGEN/ir_build_literal.h"
#include "IRGEN/ir_builder.h"
#include "IRGEN/ir_gen_expr.h"
#include "IRGEN/ir_gen_polymorphable.h"
#include "IRGEN/ir_gen_type.h"
#include "UTIL/builtin_type.h"
#include "UTIL/color.h"
#include "UTIL/ground.h"
#include "UTIL/trait.h"

ir_type_map_t ir_type_map_create(ast_t *ast, ir_module_t *module){
    ir_pool_t *pool = &module->pool;

    length_t estimate = ast->composites_length + ast->enums_length + 24;
    ir_type_map_t type_map = list_create(ir_type_map_t, ir_type_mapping_t, estimate);

    // Create type mappings for all builtin types
    ir_type_mapping_t builtin_type_mappings[] = {
        ir_type_mapping_create("bool", module->common.ir_bool),
        ir_type_mapping_create("byte", ir_type_make(pool, TYPE_KIND_S8, NULL)),
        ir_type_mapping_create("ubyte", ir_type_make(pool, TYPE_KIND_U8, NULL)),
        ir_type_mapping_create("short", ir_type_make(pool, TYPE_KIND_S16, NULL)),
        ir_type_mapping_create("ushort", ir_type_make(pool, TYPE_KIND_U16, NULL)),
        ir_type_mapping_create("int", ir_type_make(pool, TYPE_KIND_S32, NULL)),
        ir_type_mapping_create("uint", ir_type_make(pool, TYPE_KIND_U32, NULL)),
        ir_type_mapping_create("long", ir_type_make(pool, TYPE_KIND_S64, NULL)),
        ir_type_mapping_create("ulong", ir_type_make(pool, TYPE_KIND_U64, NULL)),
        ir_type_mapping_create("half", ir_type_make(pool, TYPE_KIND_HALF, NULL)),
        ir_type_mapping_create("float", ir_type_make(pool, TYPE_KIND_FLOAT, NULL)),
        ir_type_mapping_create("double", ir_type_make(pool, TYPE_KIND_DOUBLE, NULL)),
        ir_type_mapping_create("ptr", module->common.ir_ptr),
        ir_type_mapping_create("usize", module->common.ir_usize),
        ir_type_mapping_create("successful", module->common.ir_bool),
        ir_type_mapping_create("void", ir_type_make(pool, TYPE_KIND_VOID, NULL))
    };

    for(length_t i = 0; i < NUM_ITEMS(builtin_type_mappings); i++){
        ir_type_map_append(&type_map, builtin_type_mappings[i]);
    }

    // Create type mappings for every composite type.
    // Each mapping will have a placeholder value for the IR type, since the IR type of a composite
    // may depend on the type mappings of other composites.
    for(length_t i = 0; i != ast->composites_length; i++){
        ast_composite_t *composite = &ast->composites[i];
        ir_type_map_append(&type_map, ir_type_mapping_create(composite->name, ir_type_make(pool, TYPE_KIND_UNBUILT_COMPOSITE, composite)));
    }

    // Create type mappings for every enum type.
    // Since every enum is lowered to a u64, all of them will be mapped to the u64 IR type.
    for(length_t i = 0; i != ast->enums_length; i++){
        ast_enum_t *enum_definition = &ast->enums[i];
        ir_type_map_append(&type_map, ir_type_mapping_create(enum_definition->name, module->common.ir_usize));
    }

    // Sort the mappings for easily lookup later on
    qsort(type_map.mappings, type_map.length, sizeof(ir_type_mapping_t), ir_type_mapping_cmp);
    
    // Return created type map
    return type_map;
}

errorcode_t ir_gen_type_mappings(compiler_t *compiler, object_t *object){
    ast_t *ast = &object->ast;
    ir_module_t *module = &object->ir_module;

    module->type_map = ir_type_map_create(ast, module);

    ir_type_map_t *type_map = &module->type_map;

    // Ensure the identifier for each type mapping is unique
    for(length_t i = 1; i < type_map->length; i++){
        if(streq(type_map->mappings[i - 1].name, type_map->mappings[i].name)){
            // Error - Multiple types with the same name

            weak_cstr_t name = type_map->mappings[i].name;
            object_panicf_plain(object, "Multiple definitions of type '%s'", name);

            // Find every composite with that name
            for(length_t j = 0; j != ast->composites_length; j++){
                if(streq(ast->composites[j].name, name)){
                    compiler_panic(compiler, ast->composites[j].source, "Here");
                }
            }

            // Find every enum with that name
            for(length_t j = 0; j != ast->enums_length; j++){
                if(streq(ast->enums[j].name, name)){
                    compiler_panic(compiler, ast->enums[j].source, "Here");
                }
            }

            return FAILURE;
        }
    }

    // Pre-validate string type (if it exists)
    ir_type_t *ir_string_type;
    if(ir_type_map_find(type_map, "String", &ir_string_type)){
        ast_composite_t *composite = (ast_composite_t*) ir_string_type->extra;

        if(ir_string_type->kind != TYPE_KIND_UNBUILT_COMPOSITE
        || composite->layout.traits != TRAIT_NONE
        || !ast_layout_is_simple_struct(&composite->layout)
        || ast_simple_field_map_get_count(&composite->layout.field_map) != 4
        || !ast_type_is_base_ptr_of(ast_layout_skeleton_get_type_at_index(&composite->layout.skeleton, 0), "ubyte")
        || !ast_type_is_base_of(ast_layout_skeleton_get_type_at_index(&composite->layout.skeleton, 1), "usize")
        || !ast_type_is_base_of(ast_layout_skeleton_get_type_at_index(&composite->layout.skeleton, 2), "usize")
        || !ast_type_is_base_of(ast_layout_skeleton_get_type_at_index(&composite->layout.skeleton, 3), "StringOwnership")){
            compiler_panic(compiler, composite->source, "Invalid definition of built-in type 'String'");
            printf("\nShould be declared as:\n\nstruct String (\n    array *ubyte,   length usize,\n    capacity usize, ownership StringOwnership\n)\n");
            return FAILURE;
        }
    }

    // Fill in composite types
    for(length_t i = 0; i != type_map->length; i++){
        ir_type_t **mapping_type = &type_map->mappings[i].type;

        if((*mapping_type)->kind == TYPE_KIND_UNBUILT_COMPOSITE){
            ast_composite_t *composite = (*mapping_type)->extra;
            ast_layout_bone_t layout_as_bone = ast_layout_as_bone(&composite->layout);

            ir_type_t *composite_type = ast_layout_bone_to_ir_type(compiler, object, &layout_as_bone, NULL);
            if(composite_type == NULL) return FAILURE;

            // Replace by value so that the pointer remains the same
            **mapping_type = *composite_type;
        }
    }

    // Cache string IR type
    ir_type_map_find(type_map, "String", &module->common.ir_string_struct);
    return SUCCESS;
}

errorcode_t ir_gen_resolve_type(compiler_t *compiler, object_t *object, const ast_type_t *unresolved_type, ir_type_t **resolved_type){
    // NOTE: Stores resolved type into 'resolved_type'
    // NOTE: If this function fails, 'resolved_type' is not guaranteed to be the same.
    //       However, no memory will have to be manually freed after this call since
    //       everything that is allocated is allocated inside the memory pool 'object->ir_module->pool'
    // NOTE: Therefore, don't call this function if you expect it to fail, because it will pollute the pool
    //       will inactive and unused memory.
    // TODO: Add ability to handle cases with dynamic arrays etc.


    ir_module_t *ir_module = &object->ir_module;
    ir_type_map_t *type_map = &ir_module->type_map;
    
    // Peel back pointer layers
    length_t non_concrete_layers = 0;
    ast_type_t unwrapped_view = *unresolved_type;

    while(unwrapped_view.elements_length > 0){
        if(object->ir_module.rtti_collector){
            rtti_collector_mention(object->ir_module.rtti_collector, &unwrapped_view);
        }

        unsigned int element_id = unwrapped_view.elements[0]->id;

        if(element_id != AST_ELEM_FIXED_ARRAY && element_id != AST_ELEM_POINTER){
            // Found concrete element type element
            break;
        }

        unwrapped_view = ast_type_unwrapped_view(&unwrapped_view);
        non_concrete_layers++;
    }

    switch(unresolved_type->elements[non_concrete_layers]->id){
    case AST_ELEM_BASE: {
            // Apply pointers to resolved base
            char *base_name = ((ast_elem_base_t*) unresolved_type->elements[non_concrete_layers])->base;

            // Resolve type from type map
            if(!ir_type_map_find(type_map, base_name, resolved_type)){
                compiler_panicf(compiler, unresolved_type->source, "Undeclared type '%s'", base_name);
                return FAILURE;
            }
        }
        break;
    case AST_ELEM_FUNC: {
            ast_elem_func_t *function = (ast_elem_func_t*) unresolved_type->elements[non_concrete_layers];

            trait_t type_kind_func_traits = ast_func_traits_to_type_kind_func_traits(function->traits);
            ir_type_t **arg_types = ir_pool_alloc(&ir_module->pool, sizeof(ir_type_t*) * function->arity);

            for(length_t a = 0; a != function->arity; a++){
                if(ir_gen_resolve_type(compiler, object, &function->arg_types[a], &arg_types[a])) return FAILURE;
            }
            
            ir_type_t *return_type = NULL;

            if(function->return_type != NULL){
                if(ir_gen_resolve_type(compiler, object, function->return_type, &return_type)) return FAILURE;
            } else {
                return_type = ir_type_make(&ir_module->pool, TYPE_KIND_VOID, NULL);
            }

            *resolved_type = ir_type_make_function_pointer(&ir_module->pool, arg_types, function->arity, return_type, type_kind_func_traits);
        }
        break;
    case AST_ELEM_GENERIC_BASE: {
            ast_elem_generic_base_t *generic_base = (ast_elem_generic_base_t*) unresolved_type->elements[non_concrete_layers];

            // Find polymorphic structure
            ast_poly_composite_t *template = ast_poly_composite_find_exact(&object->ast, generic_base->name);

            if(template == NULL){
                compiler_panicf(compiler, generic_base->source, "Undeclared polymorphic type '%s'", generic_base->name);
                return FAILURE;
            }
            
            if(generic_base->generics_length != template->generics_length){
                const char *message = generic_base->generics_length < template->generics_length ?
                                    "Not enough type parameters specified for %s '%s' (expected %d, got %d)"
                                    : "Too many type parameters specified for %s '%s' (expected %d, got %d)";
                compiler_panicf(compiler, generic_base->source, message, ast_layout_kind_name(template->layout.kind),
                        generic_base->name, template->generics_length, generic_base->generics_length);
                return FAILURE;
            }

            ast_poly_catalog_t catalog;
            ast_poly_catalog_init(&catalog);
            ast_poly_catalog_add_types(&catalog, template->generics, generic_base->generics, template->generics_length);

            ast_layout_bone_t layout_as_bone = ast_layout_as_bone(&template->layout);
            ir_type_t *created_type = ast_layout_bone_to_ir_type(compiler, object, &layout_as_bone, &catalog);
            if(created_type == NULL){
                ast_poly_catalog_free(&catalog);
                return FAILURE;
            }

            *resolved_type = created_type;
            ast_poly_catalog_free(&catalog);
        }
        break;
    case AST_ELEM_POLYCOUNT: {
            ast_elem_polycount_t *polycount = (ast_elem_polycount_t*) unresolved_type->elements[non_concrete_layers];
            compiler_panicf(compiler, polycount->source, "Undetermined polycount '$#%s' in type", polycount->name);
            return FAILURE;
        }
        break;
    case AST_ELEM_LAYOUT: {
            ast_elem_layout_t *layout_elem = (ast_elem_layout_t*) unresolved_type->elements[non_concrete_layers];
            ast_layout_bone_t as_bone = ast_layout_as_bone(&layout_elem->layout);
            
            *resolved_type = ast_layout_bone_to_ir_type(compiler, object, &as_bone, NULL);
            if(*resolved_type == NULL) return FAILURE;
        }
        break;
    case AST_ELEM_POLYMORPH:
    case AST_ELEM_POLYMORPH_PREREQ: {
            ast_elem_polymorph_t *polymorph = (ast_elem_polymorph_t*) unresolved_type->elements[non_concrete_layers];
            strong_cstr_t typename = ast_type_str(unresolved_type);
            compiler_panicf(compiler, unresolved_type->source, "Encountered undetermined polymorphic type '$%s' in type '%s'", polymorph->name, typename);
            free(typename);
            return FAILURE;
        }
        break;
    case AST_ELEM_UNKNOWN_ENUM: {
            ast_elem_unknown_enum_t *unknown_enum = (ast_elem_unknown_enum_t*) unresolved_type->elements[non_concrete_layers];

            if(unresolved_type->elements_length == 1){
                compiler_panicf(compiler, unresolved_type->source, "Undetermined generic enum '[enum with %s]'", unknown_enum->kind_name);
            } else {
                strong_cstr_t typename = ast_type_str(unresolved_type);
                compiler_panicf(compiler, unresolved_type->source, "Undetermined generic enum '[enum with %s]' in type '%s'", unknown_enum->kind_name, typename);
                free(typename);
            }

            return FAILURE;
        }
        break;
    case AST_ELEM_ANONYMOUS_ENUM:
    case AST_ELEM_UNKNOWN_PLURAL_ENUM:
        *resolved_type = ir_module->common.ir_usize;
        break;
    default: {
            strong_cstr_t unresolved_typename = ast_type_str(unresolved_type);
            compiler_panicf(compiler, unresolved_type->source, "INTERNAL ERROR: Unknown type element id in type '%s'", unresolved_typename);
            free(unresolved_typename);
            return FAILURE;
        }
    }

    for(length_t i = non_concrete_layers; i != 0; i--){
        ir_type_t *wrapped_type = ir_pool_alloc(&ir_module->pool, sizeof(ir_type_t));
        unsigned int non_concrete_element_id = unresolved_type->elements[i - 1]->id;

        if(non_concrete_element_id == AST_ELEM_POINTER){
            wrapped_type->kind = TYPE_KIND_POINTER;
            wrapped_type->extra = *resolved_type;
        } else if(non_concrete_element_id == AST_ELEM_FIXED_ARRAY){
            ir_type_extra_fixed_array_t *fixed_array = ir_pool_alloc(&ir_module->pool, sizeof(ir_type_extra_fixed_array_t));
            fixed_array->subtype = *resolved_type;
            fixed_array->length = ((ast_elem_fixed_array_t*) unresolved_type->elements[i - 1])->length;
            wrapped_type->kind = TYPE_KIND_FIXED_ARRAY;
            wrapped_type->extra = fixed_array;
        } else {
            char *unresolved_str_rep = ast_type_str(unresolved_type);
            compiler_panicf(compiler, unresolved_type->source, "INTERNAL ERROR: Unknown non-concrete type element id in type '%s'", unresolved_str_rep);
            free(unresolved_str_rep);
            return FAILURE;
        }

        *resolved_type = wrapped_type;
    }

    return SUCCESS;
}

#define TYPE_TRAIT_POINTER     TRAIT_1 // *something
#define TYPE_TRAIT_BASE_PTR    TRAIT_2 // ptr
#define TYPE_TRAIT_FUNC_PTR    TRAIT_3 // function pointer
#define TYPE_TRAIT_INTEGER     TRAIT_4 // integer
#define TYPE_TRAIT_FIXED_ARRAY TRAIT_5 // fixed array
#define TYPE_TRAIT_BASE_ANY    TRAIT_6 // Any

static trait_t get_type_traits(const ast_type_t *ast_type){
    trait_t traits = TRAIT_NONE;
    unsigned int type_kind = ir_primitive_from_ast_type(ast_type);

    if(ast_type_is_pointer(ast_type)){
        traits |= TYPE_TRAIT_POINTER;
    } else if(ast_type_is_base_of(ast_type, "ptr")){
        traits |= TYPE_TRAIT_BASE_PTR;
    } else if(ast_type_is_func(ast_type)){
        traits |= TYPE_TRAIT_FUNC_PTR;
    } else if(ast_type_is_fixed_array(ast_type)){
        traits |= TYPE_TRAIT_FIXED_ARRAY;
    } else if(ast_type_is_base_of(ast_type, "Any")){
        traits |= TYPE_TRAIT_BASE_ANY;
    } else if(type_kind != TYPE_KIND_NONE && type_kind != TYPE_KIND_FLOAT && type_kind != TYPE_KIND_DOUBLE){
        traits |= TYPE_TRAIT_INTEGER;
    }

    return traits;
}

successful_t ast_types_conform(ir_builder_t *builder, ir_value_t **ir_value, ast_type_t *ast_from_type, ast_type_t *ast_to_type, trait_t mode){
    // NOTE: _____RETURNS TRUE ON SUCCESSFUL MERGE_____
    // NOTE: If the types are not identical, then this function will attempt to make
    //           a value of one type conform to another type (via casting)
    //       Ex. ptr and *int are different types but each can conform to the other
    // NOTE: This function can be used as a substitute for ast_types_identical if the desired
    //           behavior is conforming to a single type, for two-way casting use ast_types_merge()

    unsigned int from_type_kind = ir_primitive_from_ast_type(ast_from_type);
    unsigned int to_type_kind = ir_primitive_from_ast_type(ast_to_type);

    if(to_type_kind != TYPE_KIND_NONE && from_type_kind == to_type_kind){
        return true;
    }

    // Don't try to conform types if one or both of them is 'void'
    if(ast_type_is_base_of(ast_from_type, "void") || ast_type_is_base_of(ast_to_type, "void")){
        return false;
    }

    trait_t to_traits = get_type_traits(ast_to_type);
    trait_t from_traits = get_type_traits(ast_from_type);

    ir_type_t *ir_to_type;
    
    // Integer or pointer to boolean
    if(to_type_kind == TYPE_KIND_BOOLEAN && (
        (mode & CONFORM_MODE_INT_TO_BOOL && from_traits & TYPE_TRAIT_INTEGER) ||
        (mode & CONFORM_MODE_PTR_TO_BOOL && (from_traits & TYPE_TRAIT_POINTER || from_traits & TYPE_TRAIT_BASE_PTR || from_traits & TYPE_TRAIT_FUNC_PTR))
    )){
        if(ir_gen_resolve_type(builder->compiler, builder->object, ast_to_type, &ir_to_type)) return false;

        *ir_value = build_nonconst_cast(builder, INSTRUCTION_ISNTZERO, *ir_value, ir_to_type);
        return true;
    }

    // Do bitcast if appropriate
    if(
        ((
            (to_traits & TYPE_TRAIT_BASE_PTR && from_traits & TYPE_TRAIT_POINTER)  || // '*something' and 'ptr'
            (to_traits & TYPE_TRAIT_FUNC_PTR && from_traits & TYPE_TRAIT_BASE_PTR)    // function pointer and 'ptr'
            || (mode & CONFORM_MODE_POINTERPTR && ( // Allow 'ptr' to '*Something'
                (from_traits & TYPE_TRAIT_BASE_PTR && to_traits & TYPE_TRAIT_POINTER)  || // 'ptr' and '*something'
                (from_traits & TYPE_TRAIT_FUNC_PTR && to_traits & TYPE_TRAIT_BASE_PTR)    // 'ptr' and function pointer
            ))
        )) || (mode & CONFORM_MODE_POINTERS && to_traits & TYPE_TRAIT_POINTER && from_traits & TYPE_TRAIT_POINTER
            && !ast_types_identical(ast_from_type, ast_to_type) // '*something' && '*somethingelse'
        )
    ){
        // Casting pointers
        if(ir_gen_resolve_type(builder->compiler, builder->object, ast_to_type, &ir_to_type)) return false;

        *ir_value = build_bitcast(builder, *ir_value, ir_to_type);
        return true;
    }

    // Attempt to conform integers and pointers if allowed
    if(mode & CONFORM_MODE_INTPTR){
        if(from_traits & TYPE_TRAIT_INTEGER && (to_traits & TYPE_TRAIT_POINTER || to_traits & TYPE_TRAIT_BASE_PTR)){
            if(ir_gen_resolve_type(builder->compiler, builder->object, ast_to_type, &ir_to_type)) return false;

            *ir_value = build_inttoptr(builder, *ir_value, ir_to_type);
            return true;
        }
        else if(to_traits & TYPE_TRAIT_INTEGER && (from_traits & TYPE_TRAIT_POINTER || from_traits & TYPE_TRAIT_BASE_PTR)){
            if(ir_gen_resolve_type(builder->compiler, builder->object, ast_to_type, &ir_to_type)) return false;

            *ir_value = build_ptrtoint(builder, *ir_value, ir_to_type);
            return true;
        }
    }

    // Attempt to conform primitives if allowed
    if(mode & CONFORM_MODE_PRIMITIVES && from_type_kind != TYPE_KIND_NONE && to_type_kind != TYPE_KIND_NONE){
        bool from_is_float = global_type_kind_is_float[from_type_kind];
        bool to_is_float   = global_type_kind_is_float[to_type_kind];

        if(from_is_float == to_is_float){
            // They are either both floats or both integers
            // They are of different primitive types

            if(ir_gen_resolve_type(builder->compiler, builder->object, ast_to_type, &ir_to_type)) return false;

            length_t from_size = global_type_kind_sizes_in_bits_64[from_type_kind];
            length_t to_size   = global_type_kind_sizes_in_bits_64[to_type_kind];

            // Decide what type of cast it should be
            if(from_size == to_size){
                *ir_value = build_reinterpret(builder, *ir_value, ir_to_type);
            } else if(from_size < to_size){
                if(from_is_float){
                    *ir_value = build_fext(builder, *ir_value, ir_to_type);
                } else if(global_type_kind_signs[to_type_kind] && global_type_kind_signs[from_type_kind]){
                    *ir_value = build_sext(builder, *ir_value, ir_to_type);
                } else {
                    *ir_value = build_zext(builder, *ir_value, ir_to_type);
                }
            } else {
                *ir_value = from_is_float ? build_ftrunc(builder, *ir_value, ir_to_type) : build_trunc(builder, *ir_value, ir_to_type);
            }

            return true;
        }

        if(mode & CONFORM_MODE_INTFLOAT){
            // Convert int to float or float to int
            bool from_is_float = (from_type_kind == TYPE_KIND_FLOAT || from_type_kind == TYPE_KIND_DOUBLE);
            bool from_is_signed = global_type_kind_signs[from_type_kind];
            bool to_is_signed = global_type_kind_signs[to_type_kind];

            if(ir_gen_resolve_type(builder->compiler, builder->object, ast_to_type, &ir_to_type)) return false;

            // Decide what type of cast it should be
            if(from_is_float){
                *ir_value = to_is_signed ? build_fptosi(builder, *ir_value, ir_to_type) : build_fptoui(builder, *ir_value, ir_to_type);
            } else {
                *ir_value = from_is_signed ? build_sitofp(builder, *ir_value, ir_to_type) : build_uitofp(builder, *ir_value, ir_to_type);
            }

            return true;
        }
    }

    if(mode & CONFORM_MODE_INTENUM){
        // Convert enum to integer
        if((*ir_value)->type->kind == TYPE_KIND_U64 && to_traits & TYPE_TRAIT_INTEGER){
            // Don't bother casting the value when they are the same underlying type
            if(to_type_kind == TYPE_KIND_U64) return true;

            if(ir_gen_resolve_type(builder->compiler, builder->object, ast_to_type, &ir_to_type)) return false;

            bool is_same_size = global_type_kind_sizes_in_bits_64[from_type_kind] == global_type_kind_sizes_in_bits_64[to_type_kind];
            *ir_value = is_same_size ? build_reinterpret(builder, *ir_value, ir_to_type) : build_trunc(builder, *ir_value, ir_to_type);
            return true;
        }
    }

    if((mode & CONFORM_MODE_FROM_ANY) && (from_traits & TYPE_TRAIT_BASE_ANY)){
        if(to_traits & TYPE_TRAIT_BASE_ANY) return true;
    }

    if(to_traits & TYPE_TRAIT_BASE_ANY && !(builder->compiler->traits & COMPILER_NO_TYPEINFO)){
        if(from_traits & TYPE_TRAIT_BASE_ANY) return true;

        ir_type_t *any_type;
        ir_value_t **values = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * 2);

        ir_type_t *placeholder_actual_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t*));
        placeholder_actual_type->kind = TYPE_KIND_U64;

        // type *AnyType
        values[0] = rtti_for(builder, ast_from_type, ast_to_type->source);
        if(values[0] == NULL) return false;

        // placeholder ulong
        if(ast_from_type->elements_length != 0) switch(ast_from_type->elements[0]->id){
        case AST_ELEM_POINTER:
            values[1] = build_ptrtoint(builder, *ir_value, placeholder_actual_type);
            break;
        case AST_ELEM_BASE: {
                ast_elem_base_t *base_elem = (ast_elem_base_t*) ast_from_type->elements[0];
                maybe_index_t recognized = typename_builtin_type(base_elem->base);

                switch(recognized){
                case BUILTIN_TYPE_NONE:
                    // Try to store a pointer to whatever the data is
                    // TODO: Implement converting between non-built-in types and Any
                    // redprintf("converting non-built-in types to Any has not been implemented yet!\n");
                    return false;
                case BUILTIN_TYPE_SHORT:
                case BUILTIN_TYPE_USHORT:
                    values[1] = build_zext(builder, *ir_value, placeholder_actual_type);
                    break;
                case BUILTIN_TYPE_INT:
                case BUILTIN_TYPE_UINT:
                    values[1] = build_zext(builder, *ir_value, placeholder_actual_type);
                    break;
                case BUILTIN_TYPE_BOOL:
                case BUILTIN_TYPE_BYTE:
                case BUILTIN_TYPE_UBYTE:
                case BUILTIN_TYPE_SUCCESSFUL:
                    values[1] = build_zext(builder, *ir_value, placeholder_actual_type);
                    break;
                case BUILTIN_TYPE_FLOAT: {
                        ir_type_t *uint_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t*));
                        uint_type->kind = TYPE_KIND_U32;
                        values[1] = build_zext(builder, build_bitcast(builder, *ir_value, uint_type), placeholder_actual_type);
                    }
                    break;
                case BUILTIN_TYPE_LONG:
                case BUILTIN_TYPE_DOUBLE:
                    // Bitcasting works fine here since they're both 64 bits
                    values[1] = build_bitcast(builder, *ir_value, placeholder_actual_type);
                    break;
                case BUILTIN_TYPE_ULONG:
                case BUILTIN_TYPE_USIZE:
                    // No conversion necessary because they are already the same type as the placeholder
                    values[1] = *ir_value;
                    break;
                default:
                    values[1] = build_literal_usize(builder->pool, 0);
                }
            }
            break;
        default:
            values[1] = build_literal_usize(builder->pool, 0);
        }

        if(!ir_type_map_find(builder->type_map, "Any", &any_type)){
            internalerrorprintf("ast_types_conform() - Failed to find critical 'Any' type used by the runtime type table that should exist\n");
            return false;
        }

        // Convert RTTI pointer to be s8* since that's how pointers inside of composite are stored
        values[0] = build_bitcast(builder, values[0], builder->object->ir_module.common.ir_ptr);
        
        *ir_value = build_const_struct_literal(builder->pool, any_type, values, 2);
        return true;
    }

    if(mode & CONFORM_MODE_INTENUM && from_traits & TYPE_TRAIT_INTEGER){
        if(ir_gen_resolve_type(builder->compiler, builder->object, ast_to_type, &ir_to_type)) return false;

        // If the type boils down to a u64 we'll consider it an enum
        bool to_type_is_enum = ir_to_type->kind == TYPE_KIND_U64;

        // Convert integer to enum
        if(to_type_is_enum){
            // Don't bother casting the value when they are the same underlying type
            if(from_type_kind == TYPE_KIND_U64) return true;

            // Since enums aren't standard type kinds, we have to use '64' instead of global_type_kind_sizes_in_bits_64
            bool is_same_size = 64 == global_type_kind_sizes_in_bits_64[from_type_kind];
            *ir_value = is_same_size ? build_reinterpret(builder, *ir_value, ir_to_type) : build_zext(builder, *ir_value, ir_to_type);
            return true;
        }
    }

    if(ast_types_identical(ast_from_type, ast_to_type)) return true;

    // Handle automatic class pointer casts
    if((mode & CONFORM_MODE_CLASS_POINTERS) && ast_type_is_pointer_to_base_like(ast_from_type) && ast_type_is_pointer_to_base_like(ast_to_type)){
        ast_type_t potential_child = ast_type_dereferenced_view(ast_from_type);
        ast_type_t potential_parent = ast_type_dereferenced_view(ast_to_type);

        ast_poly_catalog_t catalog;
        ast_poly_catalog_init(&catalog);
        bool does_extend = ir_gen_does_extend(builder->compiler, builder->object, &potential_child, &potential_parent, &catalog);
        ast_poly_catalog_free(&catalog);

        if(does_extend){
            if(ir_gen_resolve_type(builder->compiler, builder->object, ast_to_type, &ir_to_type)) return false;

            *ir_value = build_bitcast(builder, *ir_value, ir_to_type);
            return true;
        }
    }

    // Handle conforming of values of unknown enum types to concrete enum values
    if(ast_type_is_base(ast_to_type) && ast_type_is_unknown_enum(ast_from_type)){
        weak_cstr_t maybe_enum_name = ((ast_elem_base_t*) ast_to_type->elements[0])->base;
        ast_elem_unknown_enum_t *unknown_enum = (ast_elem_unknown_enum_t*) ast_from_type->elements[0];

        ast_t *ast = &builder->object->ast;
        maybe_index_t enum_index = ast_find_enum(ast->enums, ast->enums_length, maybe_enum_name);

        if(enum_index >= 0 && ast_enum_contains(&ast->enums[enum_index], unknown_enum->kind_name)){
            *ir_value = ir_gen_actualize_unknown_enum(builder->compiler, builder->object, maybe_enum_name, unknown_enum->kind_name, unknown_enum->source, NULL);
            assert(*ir_value != NULL);
            return true;
        }
    }

    // Handle conforming of values of unknown plural enum types to concrete enum values
    if(ast_type_is_base(ast_to_type) && ast_type_is_unknown_plural_enum(ast_from_type)){
        const char *enum_name = ast_type_base_name(ast_to_type);
        ast_elem_unknown_plural_enum_t *from_unknown_enum = (ast_elem_unknown_plural_enum_t*) ast_from_type->elements[0];

        if(ir_gen_actualize_unknown_plural_enum(builder, enum_name, &from_unknown_enum->kinds, ir_value, ast_to_type->source) == SUCCESS){
            return true;
        }
    }

    // Handle conforming of values of unknown plural enum types to concrete anonymous enum values
    if(ast_type_is_anonymous_enum(ast_to_type) && ast_type_is_unknown_plural_enum(ast_from_type)){
        ast_elem_anonymous_enum_t *elem = (ast_elem_anonymous_enum_t*) ast_to_type->elements[0];
        ast_elem_unknown_plural_enum_t *from_unknown_enum = (ast_elem_unknown_plural_enum_t*) ast_from_type->elements[0];

        if(ir_gen_actualize_unknown_plural_enum_to_anonymous(builder, &elem->kinds, &from_unknown_enum->kinds, ir_value, ast_to_type->source) == SUCCESS){
            return true;
        }
    }
    
    if(ast_type_is_anonymous_enum(ast_to_type) && ast_type_is_unknown_enum(ast_from_type)){
        ast_elem_anonymous_enum_t *anonymous_enum = (ast_elem_anonymous_enum_t*) ast_to_type->elements[0];
        ast_elem_unknown_enum_t *unknown_enum = (ast_elem_unknown_enum_t*) ast_from_type->elements[0];

        maybe_index_t member_index = ast_elem_anonymous_enum_get_member_index(anonymous_enum, unknown_enum->kind_name);

        if(member_index >= 0){
            *ir_value = build_literal_usize(builder->pool, member_index);
            assert(*ir_value != NULL);
            return true;
        }
    }

    // Worst case scenario, we try to use user-defined __as__ method
    if((mode & CONFORM_MODE_USER_IMPLICIT || mode & CONFORM_MODE_USER_EXPLICIT)){
        bool can_do = (ast_type_is_base_like(ast_from_type) || ast_type_is_fixed_array(ast_from_type)) &&
                      (ast_type_is_base_like(ast_to_type) || ast_type_is_fixed_array(ast_to_type));

        if(can_do){
            ast_expr_phantom_t phantom_value = {
                .id = EXPR_PHANTOM,
                .ir_value = *ir_value,
                .source = NULL_SOURCE,
                .is_mutable = false,
                .type = ast_type_clone(ast_from_type),
            };

            ast_expr_t *args = (ast_expr_t*) &phantom_value;

            // DANGEROUS: Sharing 'gives' memory with existing type,
            // we must reset 'as_call.gives.elements_length' to zero before
            // freeing it
            // DANGEROUS: Using stack-allocated argument expressions,
            // we must reset 'arity' to zero and 'args' to NULL before
            // freeing it
            // DANGEROUS: Using constant string for strong_cstr_t name,
            // we must reset 'name' to be NULL before freeing it
            ast_expr_call_t as_call;
            ast_expr_create_call_in_place(&as_call, "__as__", 1, &args, true, ast_to_type, NULL_SOURCE);

            // Mark special flag 'only_implicit' if explicit user-defined conversions
            // aren't allowed for this particular cast
            as_call.only_implicit = !(mode & CONFORM_MODE_USER_EXPLICIT);
            as_call.no_user_casts = true;
            
            ir_value_t *converted;
            ast_type_t temporary_type;
            errorcode_t error = ir_gen_expr(builder, (ast_expr_t*) &as_call, &converted, false, &temporary_type);

            as_call.name = NULL;
            as_call.args = NULL;
            as_call.arity = 0;
            as_call.gives.elements_length = 0;
            ast_expr_free((ast_expr_t*) &as_call);

            ast_type_free(&phantom_value.type);

            // Ensure tentative call was successful
            if(error || ast_type_is_void(&temporary_type)){
                ast_type_free(&temporary_type);
                return false;
            }
            
            ast_type_free(&temporary_type);

            *ir_value = converted;
            return true;
        }
    }

    return false;
}

static ir_value_t *ir_gen_index_value_for_kind(ir_pool_t *pool, const strong_cstr_list_t *kinds, const char *kind_name){
    maybe_index_t index = strong_cstr_list_bsearch(kinds, kind_name);

    if(index >= 0){
        return build_literal_usize(pool, index);
    } else {
        return NULL;
    }
}

errorcode_t ir_gen_merge_unknown_enum_like_into_plural_unknown_enum(ir_builder_t *builder, ir_value_t **ir_value, const ast_type_t *ast_type, const ast_type_t *merged_type){
    ir_pool_snapshot_t pool_snapshot = ir_pool_snapshot_capture(builder->pool);

    assert(ast_type_is_unknown_plural_enum(merged_type));
    ast_elem_unknown_plural_enum_t *to_plural = (ast_elem_unknown_plural_enum_t*) merged_type->elements[0];

    if(ast_type_is_unknown_enum(ast_type)){
        const char *kind_name = ((ast_elem_unknown_enum_t*) ast_type->elements[0])->kind_name;

        ir_value_t *index_value = ir_gen_index_value_for_kind(builder->pool, &to_plural->kinds, kind_name);
        if(index_value == NULL) goto failure;

        *ir_value = index_value;
        return SUCCESS;
    } else if(ast_type_is_unknown_plural_enum(ast_type)){
        ast_elem_unknown_plural_enum_t *from_plural = (ast_elem_unknown_plural_enum_t*) ast_type->elements[0];
        
        length_t count = from_plural->kinds.length;
        ir_value_t **values = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * count);
        ir_type_t *item_type = builder->object->ir_module.common.ir_usize;

        for(length_t i = 0; i < count; i++){
            const char *kind_name = from_plural->kinds.items[i];

            values[i] = ir_gen_index_value_for_kind(builder->pool, &to_plural->kinds, kind_name);
            if(values[i] == NULL) goto failure;
        }

        ir_value_t *map = build_array_literal(builder->pool, item_type, values, count);
        *ir_value = build_load(builder, build_array_access(builder, map, *ir_value, ast_type->source), ast_type->source);
        return SUCCESS;
    }

    strong_cstr_t a_str, merged_str;

failure:
    a_str = ast_type_str(ast_type);
    merged_str = ast_type_str(merged_type);
    compiler_panicf(builder->compiler, ast_type->source, "Failed to merge unknown enum value(s) from type '%s' into '%s'", a_str, merged_str);
    free(merged_str);
    free(a_str);

    ir_pool_snapshot_restore(builder->pool, &pool_snapshot);
    return FAILURE;
}

successful_t ast_types_merge(ir_builder_t *builder, ir_value_t **ir_value_a, ir_value_t **ir_value_b, ast_type_t *ast_type_a, ast_type_t *ast_type_b){
    // NOTE: _____RETURNS TRUE ON SUCCESSFUL MERGE_____
    // NOTE: If the types are not identical, then this function will attempt to make
    //           the two values have a common type
    // NOTE: This is basically a two-way version of ast_types_conform()
    // NOTE: This function can be used as a substitute for ast_types_identical if the desired
    //           behavior is to make two values conform to each other, for one-way casting use ast_types_conform()

    if(ast_types_conform(builder, ir_value_a, ast_type_a, ast_type_b, CONFORM_MODE_STANDARD)) return false;
    if(ast_types_conform(builder, ir_value_b, ast_type_b, ast_type_a, CONFORM_MODE_STANDARD)) return false;
    return true;
}

ir_type_t *ast_layout_bone_to_ir_type(compiler_t *compiler, object_t *object, ast_layout_bone_t *bone, ast_poly_catalog_t *optional_catalog){
    // Returns NULL when something goes wrong

    // Handle AST Type bones
    if(bone->kind == AST_LAYOUT_BONE_KIND_TYPE){
        ir_type_t *result;

        if(ast_type_is_pointer(&bone->type)){
            // Use 'ptr' instead of any other pointer types inside of structs,
            // in order to allow for circular pointer references.
            // When fields are accessed, they will casted to the proper type

            return object->ir_module.common.ir_ptr;
        }

        if(optional_catalog){
            ast_type_t resolved_ast_type;

            if(ast_resolve_type_polymorphs(compiler, object->ir_module.rtti_collector, optional_catalog, &bone->type, &resolved_ast_type)){
                return NULL;
            }

            if(ir_gen_resolve_type(compiler, object, &resolved_ast_type, &result)){
                ast_type_free(&resolved_ast_type);
                return NULL;
            }

            ast_type_free(&resolved_ast_type);
        } else if(ir_gen_resolve_type(compiler, object, &bone->type, &result)){
            return NULL;
        }

        return result;
    }

    // Handle bones that have children
    ir_pool_t *pool = &object->ir_module.pool;
    ir_type_t *result = ir_pool_alloc(pool, sizeof(ir_type_t));
    ir_type_extra_composite_t *extra = ir_pool_alloc(pool, sizeof(ir_type_extra_composite_t));
    extra->subtypes = ir_pool_alloc(pool, sizeof(ir_type_t*) * bone->children.bones_length);
    extra->subtypes_length = bone->children.bones_length;
    extra->traits = bone->traits & AST_LAYOUT_BONE_PACKED;

    for(length_t i = 0; i != bone->children.bones_length; i++){
        ir_type_t *subtype = ast_layout_bone_to_ir_type(compiler, object, &bone->children.bones[i], optional_catalog);
        if(subtype == NULL) return NULL;
        extra->subtypes[i] = subtype;
    }

    switch(bone->kind){
    case AST_LAYOUT_BONE_KIND_UNION:
        result->kind = TYPE_KIND_UNION;
        break;
    case AST_LAYOUT_BONE_KIND_STRUCT:
        result->kind = TYPE_KIND_STRUCTURE;
        break;
    default:
        die("ast_layout_bone_to_ir_type() - Unrecognized bone kind %d\n", (int) bone->kind);
    }
    
    result->extra = extra;
    return result;
}

ir_value_t *ast_layout_path_get_offset(ir_module_t *ir_module, ast_layout_endpoint_t *endpoint, ast_layout_endpoint_path_t *path, ir_type_t *root_ir_type){
    // NOTE: Returns NULL on failure

    ir_pool_t *pool = &ir_module->pool;
    ir_type_t *usize_type = ir_module->common.ir_usize;

    ir_value_t *offset = build_literal_usize(pool, 0);
    ir_type_t *working_root_ir_type = root_ir_type;

    for(length_t i = 0; i != AST_LAYOUT_MAX_DEPTH && path->waypoints[i].kind != AST_LAYOUT_WAYPOINT_END; i++){
        ast_layout_waypoint_t waypoint = path->waypoints[i];

        if(waypoint.kind == AST_LAYOUT_WAYPOINT_OFFSET){
            ir_value_t *relative_offset = build_offsetof_ex(pool, usize_type, working_root_ir_type, waypoint.index);
            offset = build_const_add(pool, offset, relative_offset);
        }

        ir_type_extra_composite_t *extra_info = (ir_type_extra_composite_t*) working_root_ir_type->extra;
        working_root_ir_type = extra_info->subtypes[endpoint->indices[i]];
    }

    return offset;
}

int ir_type_mapping_cmp(const void *a, const void *b){
    return strcmp(((ir_type_mapping_t*) a)->name, ((ir_type_mapping_t*) b)->name);
}

unsigned int ir_primitive_from_ast_type(const ast_type_t *type){
    // NOTE: Returns TYPE_KIND_NONE when no suitable fit primitive

    if(type->elements_length != 1) return TYPE_KIND_NONE;
    if(type->elements[0]->id != AST_ELEM_BASE) return TYPE_KIND_NONE;

    char *base = ((ast_elem_base_t*) type->elements[0])->base;

    maybe_index_t builtin = typename_builtin_type(base);

    switch(builtin){
    case BUILTIN_TYPE_BOOL:       return TYPE_KIND_BOOLEAN;
    case BUILTIN_TYPE_BYTE:       return TYPE_KIND_S8;
    case BUILTIN_TYPE_DOUBLE:     return TYPE_KIND_DOUBLE;
    case BUILTIN_TYPE_FLOAT:      return TYPE_KIND_FLOAT;
    case BUILTIN_TYPE_INT:        return TYPE_KIND_S32;
    case BUILTIN_TYPE_LONG:       return TYPE_KIND_S64;
    case BUILTIN_TYPE_SHORT:      return TYPE_KIND_S16;
    case BUILTIN_TYPE_SUCCESSFUL: return TYPE_KIND_BOOLEAN;
    case BUILTIN_TYPE_UBYTE:      return TYPE_KIND_U8;
    case BUILTIN_TYPE_UINT:       return TYPE_KIND_U32;
    case BUILTIN_TYPE_ULONG:      return TYPE_KIND_U64;
    case BUILTIN_TYPE_USHORT:     return TYPE_KIND_U16;
    case BUILTIN_TYPE_USIZE:      return TYPE_KIND_U64;
    case BUILTIN_TYPE_NONE:       return TYPE_KIND_NONE;
    }

    return TYPE_KIND_NONE; // Should never be reached
}
