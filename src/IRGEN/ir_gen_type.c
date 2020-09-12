
#include "UTIL/color.h"
#include "UTIL/ground.h"
#include "UTIL/search.h"
#include "UTIL/filename.h"
#include "UTIL/builtin_type.h"
#include "IRGEN/ir_gen.h"
#include "IRGEN/ir_gen_type.h"
#include "BRIDGE/any.h"
#include "BRIDGE/rtti.h"

errorcode_t ir_gen_type_mappings(compiler_t *compiler, object_t *object){
    // NOTE: Maps all accessible types in the program

    ast_t *ast = &object->ast;
    ir_module_t *module = &object->ir_module;
    ir_pool_t *pool = &module->pool;

    // Base types:
    // byte, ubyte, short, ushort, int, uint, long, ulong, half, float, double, bool, ptr, usize, successful, void
    #define IR_GEN_BASE_TYPE_MAPPINGS_COUNT 16

    ir_type_t *tmp_type;
    ir_type_map_t *type_map = &module->type_map;
    type_map->mappings_length = ast->structs_length + ast->enums_length + IR_GEN_BASE_TYPE_MAPPINGS_COUNT;
    ir_type_mapping_t *mappings = malloc(sizeof(ir_type_mapping_t) * type_map->mappings_length);

    mappings[0].name = "byte";
    mappings[0].type.kind = TYPE_KIND_S8;
    // <dont bother setting 'mappings[0].type.extra' because it will never be accessed>
    mappings[1].name = "ubyte";
    mappings[1].type.kind = TYPE_KIND_U8;
    mappings[2].name = "short";
    mappings[2].type.kind = TYPE_KIND_S16;
    mappings[3].name = "ushort";
    mappings[3].type.kind = TYPE_KIND_U16;
    mappings[4].name = "int";
    mappings[4].type.kind = TYPE_KIND_S32;
    mappings[5].name = "uint";
    mappings[5].type.kind = TYPE_KIND_U32;
    mappings[6].name = "long";
    mappings[6].type.kind = TYPE_KIND_S64;
    mappings[7].name = "ulong";
    mappings[7].type.kind = TYPE_KIND_U64;
    mappings[8].name = "half";
    mappings[8].type.kind = TYPE_KIND_HALF;
    mappings[9].name = "float";
    mappings[9].type.kind = TYPE_KIND_FLOAT;
    mappings[10].name = "double";
    mappings[10].type.kind = TYPE_KIND_DOUBLE;
    mappings[11].name = "bool";
    mappings[11].type.kind = TYPE_KIND_BOOLEAN;
    mappings[12].name = "ptr";
    mappings[12].type.kind = TYPE_KIND_POINTER;
    tmp_type = ir_pool_alloc(pool, sizeof(ir_type_t*));
    tmp_type->kind = TYPE_KIND_S8;
    mappings[12].type.extra = tmp_type; 
    mappings[13].name = "usize";
    mappings[13].type.kind = TYPE_KIND_U64;
    mappings[14].name = "successful";
    mappings[14].type.kind = TYPE_KIND_BOOLEAN;
    mappings[15].name = "void";
    mappings[15].type.kind = TYPE_KIND_VOID;

    length_t beginning_of_structs = IR_GEN_BASE_TYPE_MAPPINGS_COUNT;
    length_t beginning_of_enums = type_map->mappings_length - ast->enums_length;

    for(length_t i = beginning_of_structs; i != beginning_of_enums; i++){
        // Create skeletons for struct type maps
        ast_struct_t *structure = &ast->structs[i - IR_GEN_BASE_TYPE_MAPPINGS_COUNT];
        mappings[i].name = structure->name; // Will live on
        mappings[i].type.kind = TYPE_KIND_STRUCTURE;

        // Temporary use mappings[i].type.extra as a pointer to reference the ast_struct_t used
        // It will later be changed to the required composite data after this mappings have been sorted
        mappings[i].type.extra = structure;
    }

    for(length_t i = beginning_of_enums; i != type_map->mappings_length; i++){
        ast_enum_t *inum = &ast->enums[i - beginning_of_enums];
        mappings[i].name = inum->name;
        mappings[i].type.kind = TYPE_KIND_U64;
    }

    qsort(mappings, type_map->mappings_length, sizeof(ir_type_mapping_t), (void*) ir_type_mapping_cmp);
    type_map->mappings = mappings;

    // EXPERIMENTAL: Make sure each identifier is unique
    for(length_t i = 1; i < type_map->mappings_length; i++){
        if(strcmp(mappings[i - 1].name, mappings[i].name) == 0){
            // ERROR: Multiple types with the same name
            weak_cstr_t name = mappings[i].name;
            object_panicf_plain(object, "Multiple definitions of type '%s'", name);

            // Find every struct with that name
            for(length_t s = 0; s != ast->structs_length; s++){
                if(strcmp(ast->structs[s].name, name) == 0)
                    compiler_panic(compiler, ast->structs[s].source, "Here");
            }

            // Find every enum with that name
            for(length_t e = 0; e != ast->enums_length; e++){
                if(strcmp(ast->enums[e].name, name) == 0)
                    compiler_panic(compiler, ast->enums[e].source, "Here");
            }

            return FAILURE;
        }
    }

    for(length_t i = 0; i != type_map->mappings_length; i++){
        // Fill in bodies for struct type maps
        if(mappings[i].type.kind != TYPE_KIND_STRUCTURE) continue;

        // NOTE: mappings[i].type.extra is used temporary to store the ast_struct_t* used
        ast_struct_t *structure = mappings[i].type.extra;

        // Special enforcement of type 'String'
        if(strcmp(structure->name, "String") == 0){
            if(structure->traits != TRAIT_NONE
            || structure->field_count != 4
            || !ast_type_is_base_ptr_of(&structure->field_types[0], "ubyte")
            || !ast_type_is_base_of(&structure->field_types[1], "usize")
            || !ast_type_is_base_of(&structure->field_types[2], "usize")
            || !ast_type_is_base_of(&structure->field_types[3], "StringOwnership")){
                compiler_panic(compiler, structure->source, "Invalid definition of built-in type 'String'");
                printf("\nShould be declared as:\n\nstruct String (\n    array *ubyte,   length usize,\n    capacity usize, ownership StringOwnership\n)\n");
                return FAILURE;
            }

            module->common.ir_string_struct = &mappings[i].type;
        }

        // Transform union structures into actual union types
        if(structure->traits & AST_STRUCT_IS_UNION) mappings[i].type.kind = TYPE_KIND_UNION;

        // NOTE: This composite information will replace mappings[i].type.extra
        ir_type_extra_composite_t *composite = ir_pool_alloc(pool, sizeof(ir_type_extra_composite_t));
        composite->subtypes_length = structure->field_count;
        composite->subtypes = ir_pool_alloc(pool, sizeof(ir_type_t*) * structure->field_count);
        composite->traits = structure->traits & AST_STRUCT_PACKED ? TYPE_KIND_COMPOSITE_PACKED : TRAIT_NONE;
        mappings[i].type.extra = composite;

        // Resolve composite subtypes
        for(length_t s = 0; s != structure->field_count; s++){
            if(ir_gen_resolve_type(compiler, object, &structure->field_types[s], &composite->subtypes[s])) return FAILURE;
        }
    }

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
    length_t non_concrete_layers;
    ir_type_map_t *type_map = &ir_module->type_map;
    
    // Peel back pointer layers
    for(non_concrete_layers = 0; non_concrete_layers != unresolved_type->elements_length; non_concrete_layers++){
        unsigned int element_id = unresolved_type->elements[non_concrete_layers]->id;
        if(element_id != AST_ELEM_FIXED_ARRAY && element_id != AST_ELEM_POINTER) break;
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
            ir_type_extra_function_t *extra = ir_pool_alloc(&ir_module->pool, sizeof(ir_type_extra_function_t));

            *resolved_type = ir_pool_alloc(&ir_module->pool, sizeof(ir_type_t));
            (*resolved_type)->kind = TYPE_KIND_FUNCPTR;
            (*resolved_type)->extra = extra;

            extra->traits = TRAIT_NONE;
            if(function->traits & AST_FUNC_VARARG)  extra->traits |= TYPE_KIND_FUNC_VARARG;
            if(function->traits & AST_FUNC_STDCALL) extra->traits |= TYPE_KIND_FUNC_STDCALL;

            extra->arity = function->arity;
            extra->arg_types = ir_pool_alloc(&ir_module->pool, sizeof(ir_type_t*) * extra->arity);

            for(length_t a = 0; a != function->arity; a++){
                if(ir_gen_resolve_type(compiler, object, &function->arg_types[a], &extra->arg_types[a])) return FAILURE;
            }

            if(ir_gen_resolve_type(compiler, object, function->return_type, &extra->return_type)) return FAILURE;
        }
        break;
    case AST_ELEM_GENERIC_BASE: {
            ast_elem_generic_base_t *generic_base = (ast_elem_generic_base_t*) unresolved_type->elements[non_concrete_layers];
            ir_type_t *created_type = ir_pool_alloc(&object->ir_module.pool, sizeof(ir_type_t));
            ir_type_extra_composite_t *extra = ir_pool_alloc(&object->ir_module.pool, sizeof(ir_type_extra_composite_t));
            created_type->extra = extra;

            // Find polymorphic structure
            ast_polymorphic_struct_t *template = ast_polymorphic_struct_find(&object->ast, generic_base->name);
            created_type->kind = template->traits & AST_STRUCT_IS_UNION ? TYPE_KIND_UNION : TYPE_KIND_STRUCTURE;

            if(template == NULL){
                compiler_panicf(compiler, generic_base->source, "Undeclared type '%s'", generic_base->name);
                return FAILURE;
            }
            
            if(generic_base->generics_length != template->generics_length){
                const char *message = generic_base->generics_length < template->generics_length ?
                                    "Not enough type parameters specified for %s '%s' (expected %d, got %d)"
                                    : "Too many type parameters specified for %s '%s' (expected %d, got %d)";
                compiler_panicf(compiler, generic_base->source, message, template->traits & AST_STRUCT_IS_UNION ? "union" : "struct",
                        generic_base->name, template->generics_length, generic_base->generics_length);
                return FAILURE;
            }

            ast_type_var_catalog_t catalog;
            ast_type_var_catalog_init(&catalog);

            for(length_t i = 0; i != template->generics_length; i++){
                ast_type_var_catalog_add(&catalog, template->generics[i], &generic_base->generics[i]);
            }

            extra->subtypes = ir_pool_alloc(&object->ir_module.pool, sizeof(ir_type_t*) * template->field_count);
            extra->subtypes_length = template->field_count;

            for(length_t i = 0; i != extra->subtypes_length; i++){
                ast_type_t tmp_ast_type;
                if(resolve_type_polymorphics(compiler, object->ast.type_table, &catalog, &template->field_types[i], &tmp_ast_type)){
                    ast_type_var_catalog_free(&catalog);
                    return FAILURE;
                }

                if(ir_gen_resolve_type(compiler, object, &tmp_ast_type, &extra->subtypes[i])){
                    ast_type_free(&tmp_ast_type);
                    ast_type_var_catalog_free(&catalog);
                    return FAILURE;
                }
                ast_type_free(&tmp_ast_type);
            }

            extra->traits = TRAIT_NONE;
            *resolved_type = created_type;
            ast_type_var_catalog_free(&catalog);
        }
        break;
    default: {
            char *unresolved_str_rep = ast_type_str(unresolved_type);
            compiler_panicf(compiler, unresolved_type->source, "INTERNAL ERROR: Unknown type element id in type '%s'", unresolved_str_rep);
            free(unresolved_str_rep);
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
        }

        *resolved_type = wrapped_type;
    }

    return SUCCESS;
}

successful_t ast_types_conform(ir_builder_t *builder, ir_value_t **ir_value, ast_type_t *ast_from_type, ast_type_t *ast_to_type, trait_t mode){
    // NOTE: _____RETURNS TRUE ON SUCCESSFUL MERGE_____
    // NOTE: If the types are not identical, then this function will attempt to make
    //           a value of one type conform to another type (via casting)
    //       Ex. ptr and *int are different types but each can conform to the other
    // NOTE: This function can be used as a substitute for ast_types_identical if the desired
    //           behavior is conforming to a single type, for two-way casting use ast_types_merge()

    ir_type_t *ir_to_type;

    // Macro to determine whether a specific base
    #define MACRO_TYPE_IS_BASE(ast_type, a) (ast_type->elements_length == 1 && ast_type->elements[0]->id == AST_ELEM_BASE && \
        strcmp(((ast_elem_base_t*) ast_type->elements[0])->base, a) == 0)
    
    // Macro to determine whether a type is 'ptr'
    #define MACRO_TYPE_IS_BASE_PTR(ast_type) MACRO_TYPE_IS_BASE(ast_type, "ptr")

    // Macro to determine whether a type is 'Any'
    #define MACRO_TYPE_IS_BASE_ANY(ast_type) MACRO_TYPE_IS_BASE(ast_type, "Any")

    // Macro to determine whether a type is a function pointer
    #define MACRO_TYPE_IS_FUNC_PTR(ast_type) (ast_type->elements_length == 1 && ast_type->elements[0]->id == AST_ELEM_FUNC)

    // Macro to determine whether a type is a '*something'
    #define MACRO_TYPE_IS_POINTER(ast_type) (ast_type->elements_length > 1 && ast_type->elements[0]->id == AST_ELEM_POINTER)

    // Macro to determine whether a type is a 'n something'
    #define MACRO_TYPE_IS_FIXED_ARRAY(ast_type) (ast_type->elements_length > 1 && ast_type->elements[0]->id == AST_ELEM_FIXED_ARRAY)

    // Traits to keep track of what properties each type has
    trait_t to_traits = TRAIT_NONE, from_traits = TRAIT_NONE;
    #define TYPE_TRAIT_POINTER     TRAIT_1 // *something
    #define TYPE_TRAIT_BASE_PTR    TRAIT_2 // ptr
    #define TYPE_TRAIT_FUNC_PTR    TRAIT_3 // function pointer
    #define TYPE_TRAIT_INTEGER     TRAIT_4 // integer
    #define TYPE_TRAIT_FIXED_ARRAY TRAIT_5 // fixed array
    #define TYPE_TRAIT_BASE_ANY    TRAIT_6 // Any

    unsigned int from_type_kind = ir_primitive_from_ast_type(ast_from_type);
    unsigned int to_type_kind = ir_primitive_from_ast_type(ast_to_type);
    if(from_type_kind == to_type_kind && from_type_kind != TYPE_KIND_NONE) return true;

    // Mark 'to_traits' depending on the contents of 'ast_to_type'
    if(MACRO_TYPE_IS_POINTER(ast_to_type)) to_traits |= TYPE_TRAIT_POINTER;
    else if(MACRO_TYPE_IS_BASE_PTR(ast_to_type)) to_traits |= TYPE_TRAIT_BASE_PTR;
    else if(MACRO_TYPE_IS_FUNC_PTR(ast_to_type)) to_traits |= TYPE_TRAIT_FUNC_PTR;
    else if(MACRO_TYPE_IS_FIXED_ARRAY(ast_to_type)) to_traits |= TYPE_TRAIT_FIXED_ARRAY;
    else if(MACRO_TYPE_IS_BASE_ANY(ast_to_type)) to_traits |= TYPE_TRAIT_BASE_ANY;
    else {
        if(to_type_kind != TYPE_KIND_NONE && to_type_kind != TYPE_KIND_FLOAT
        && to_type_kind != TYPE_KIND_DOUBLE){
            to_traits |= TYPE_TRAIT_INTEGER;
        }
    }

    // Mark 'from_traits' depending on the contents of 'ast_from_type'
    if(MACRO_TYPE_IS_POINTER(ast_from_type)) from_traits |= TYPE_TRAIT_POINTER;
    else if(MACRO_TYPE_IS_BASE_PTR(ast_from_type)) from_traits |= TYPE_TRAIT_BASE_PTR;
    else if(MACRO_TYPE_IS_FUNC_PTR(ast_from_type)) from_traits |= TYPE_TRAIT_FUNC_PTR;
    else if(MACRO_TYPE_IS_FIXED_ARRAY(ast_from_type)) from_traits |= TYPE_TRAIT_FIXED_ARRAY;
    else if(MACRO_TYPE_IS_BASE_ANY(ast_from_type)) from_traits |= TYPE_TRAIT_BASE_ANY;
    else {
        if(from_type_kind != TYPE_KIND_NONE && from_type_kind != TYPE_KIND_FLOAT
        && from_type_kind != TYPE_KIND_DOUBLE){
            from_traits |= TYPE_TRAIT_INTEGER;
        }
    }

    // Integer or pointer to boolean
    if(to_type_kind == TYPE_KIND_BOOLEAN && (
        (mode & CONFORM_MODE_INT_TO_BOOL && from_traits & TYPE_TRAIT_INTEGER) ||
        (mode & CONFORM_MODE_PTR_TO_BOOL && (from_traits & TYPE_TRAIT_POINTER || from_traits & TYPE_TRAIT_BASE_PTR || from_traits & TYPE_TRAIT_FUNC_PTR))
    )){
        if(ir_gen_resolve_type(builder->compiler, builder->object, ast_to_type, &ir_to_type)) return false;

        ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));
        instr->id = INSTRUCTION_ISNTZERO;
        instr->result_type = ir_to_type;
        instr->value = *ir_value;
        *ir_value = build_value_from_prev_instruction(builder);
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

                // For weird compatibility purposes, we have the ability to copy
                // the lower byte region to the corresponding higher byte region
                // (under normal circumstances we should never need to use this)
                #ifdef IRGEN_COMPATIBILITY_CONVERSION_BYTE_PROMOTION
                ir_value_t *reflection_mask;
                #endif

                switch(recognized){
                case BUILTIN_TYPE_NONE:
                    // Try to store a pointer to whatever the data is
                    // TODO: Implement converting between non-built-in types and Any
                    // redprintf("converting non-built-in types to Any has not been implemented yet!\n");
                    return false;
                case BUILTIN_TYPE_SHORT:
                case BUILTIN_TYPE_USHORT:
                    values[1] = build_zext(builder, *ir_value, placeholder_actual_type);

                    // For weird compatibility purposes, we have the ability to copy
                    // the lower byte region to the corresponding higher byte region
                    // (under normal circumstances we should never need to use this)
                    #ifdef IRGEN_COMPATIBILITY_CONVERSION_BYTE_PROMOTION
                    // -------------------------------------------------------------
                    // Copy the lower 16 bits into the upper 16 bits
                    // to ensure data access won't be affected by zext'ing
                    // -------------------------------------------------------------
                    
                    // x = x + x + x * 0xFFFFFFFFFFFFul (6 bytes of 1s)
                    reflection_mask = build_math(builder, INSTRUCTION_MULTIPLY, values[1], build_literal_usize(builder->pool, 0xFFFFFFFFFFFFul), placeholder_actual_type);
                    values[1] = build_math(builder, INSTRUCTION_ADD, values[1], values[1], placeholder_actual_type);
                    values[1] = build_math(builder, INSTRUCTION_ADD, reflection_mask, values[1], placeholder_actual_type);
                    #endif
                    break;
                case BUILTIN_TYPE_INT:
                case BUILTIN_TYPE_UINT:
                    values[1] = build_zext(builder, *ir_value, placeholder_actual_type);

                    // For weird compatibility purposes, we have the ability to copy
                    // the lower byte region to the corresponding higher byte region
                    // (under normal circumstances we should never need to use this)
                    #ifdef IRGEN_COMPATIBILITY_CONVERSION_BYTE_PROMOTION
                    // -------------------------------------------------------------
                    // Copy the lower 32 bits into the upper 32 bits
                    // to ensure data access won't be affected by zext'ing
                    // -------------------------------------------------------------
                    
                    // x = x + x + x * 0xFFFFFFFF (4 bytes of 1s)
                    reflection_mask = build_math(builder, INSTRUCTION_MULTIPLY, values[1], build_literal_usize(builder->pool, 0xFFFFFFFF), placeholder_actual_type);
                    values[1] = build_math(builder, INSTRUCTION_ADD, values[1], values[1], placeholder_actual_type);
                    values[1] = build_math(builder, INSTRUCTION_ADD, reflection_mask, values[1], placeholder_actual_type);
                    #endif
                    break;
                case BUILTIN_TYPE_BOOL:
                case BUILTIN_TYPE_BYTE:
                case BUILTIN_TYPE_UBYTE:
                case BUILTIN_TYPE_SUCCESSFUL:
                    values[1] = build_zext(builder, *ir_value, placeholder_actual_type);

                    // For weird compatibility purposes, we have the ability to copy
                    // the lower byte region to the corresponding higher byte region
                    // (under normal circumstances we should never need to use this)
                    #ifdef IRGEN_COMPATIBILITY_CONVERSION_BYTE_PROMOTION
                    // -------------------------------------------------------------
                    // Copy the lowest 8 bits into the highest 8 bits
                    // to ensure data access won't be affected by zext'ing
                    // -------------------------------------------------------------

                    // x = x + x + x * 0xFFFFFFFFFFFFFFul (7 bytes of 1s)
                    reflection_mask = build_math(builder, INSTRUCTION_MULTIPLY, values[1], build_literal_usize(builder->pool, 0xFFFFFFFFFFFFFFul), placeholder_actual_type);
                    values[1] = build_math(builder, INSTRUCTION_ADD, values[1], values[1], placeholder_actual_type);
                    values[1] = build_math(builder, INSTRUCTION_ADD, reflection_mask, values[1], placeholder_actual_type);
                    #endif
                    break;
                case BUILTIN_TYPE_FLOAT: {
                        ir_type_t *uint_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t*));
                        uint_type->kind = TYPE_KIND_U32;
                        values[1] = build_zext(builder, build_bitcast(builder, *ir_value, uint_type), placeholder_actual_type);

                        // For weird compatibility purposes, we have the ability to copy
                        // the lower byte region to the corresponding higher byte region
                        // (under normal circumstances we should never need to use this)
                        #ifdef IRGEN_COMPATIBILITY_CONVERSION_BYTE_PROMOTION
                        // -------------------------------------------------------------
                        // Copy the lower 32 bits into the upper 32 bits
                        // to ensure data access won't be affected by zext'ing
                        // -------------------------------------------------------------
                        
                        // x = x + x + x * 0xFFFFFFFF (4 bytes of 1s)
                        reflection_mask = build_math(builder, INSTRUCTION_MULTIPLY, values[1], build_literal_usize(builder->pool, 0xFFFFFFFF), placeholder_actual_type);
                        values[1] = build_math(builder, INSTRUCTION_ADD, values[1], values[1], placeholder_actual_type);
                        values[1] = build_math(builder, INSTRUCTION_ADD, reflection_mask, values[1], placeholder_actual_type);
                        #endif
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
            redprintf("INTERNAL ERROR: Failed to find 'Any' type used by the runtime type table that should've been injected\n");
            return false;
        }
        
        *ir_value = build_struct_construction(builder->pool, any_type, values, 2);
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

    #undef TYPE_TRAIT_POINTER
    #undef TYPE_TRAIT_BASE_PTR
    #undef TYPE_TRAIT_FUNC_PTR
    #undef TYPE_TRAIT_INTEGER

    #undef MACRO_TYPE_IS_BASE_PTR
    #undef MACRO_TYPE_IS_FUNC_PTR
    #undef MACRO_TYPE_IS_POINTER

    return ast_types_identical(ast_from_type, ast_to_type);
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
