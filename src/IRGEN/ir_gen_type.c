
#include "UTIL/ground.h"
#include "UTIL/search.h"
#include "UTIL/filename.h"
#include "UTIL/builtin_type.h"
#include "IRGEN/ir_gen.h"
#include "IRGEN/ir_gen_type.h"

errorcode_t ir_gen_type_mappings(compiler_t *compiler, object_t *object){
    // NOTE: Maps all accessible types in the program

    ast_t *ast = &object->ast;
    ir_module_t *module = &object->ir_module;
    ir_pool_t *pool = &module->pool;

    // Base types:
    // byte, ubyte, short, ushort, int, uint, long, ulong, half, float, double, bool, ptr, usize, successful
    #define IR_GEN_BASE_TYPE_MAPPINGS_COUNT 15

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

    for(length_t i = 0; i != type_map->mappings_length; i++){
        // Fill in bodies for struct type maps
        if(mappings[i].type.kind != TYPE_KIND_STRUCTURE) continue;

        // NOTE: mappings[i].type.extra is used temporary to store the ast_struct_t* used
        ast_struct_t *structure = mappings[i].type.extra;

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

errorcode_t ir_gen_resolve_type(compiler_t *compiler, object_t *object, ast_type_t *unresolved_type, ir_type_t **resolved_type){
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

    if(unresolved_type->elements_length == 1 && unresolved_type->elements[0]->id == AST_ELEM_BASE && strcmp(((ast_elem_base_t*) unresolved_type->elements[0])->base, "void") == 0){
        // Special void type
        *resolved_type = ir_pool_alloc(&ir_module->pool, sizeof(ir_type_t));
        (*resolved_type)->kind = TYPE_KIND_VOID;
        return SUCCESS;
    }

    // Peel back pointer layers
    for(non_concrete_layers = 0; non_concrete_layers != unresolved_type->elements_length; non_concrete_layers++){
        unsigned int element_id = unresolved_type->elements[non_concrete_layers]->id;
        if(element_id == AST_ELEM_BASE || element_id == AST_ELEM_FUNC) break;
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

    // Macro to determine whether a type is 'ptr'
    #define MACRO_TYPE_IS_BASE_PTR(ast_type) (ast_type->elements_length == 1 && ast_type->elements[0]->id == AST_ELEM_BASE && \
        strcmp(((ast_elem_base_t*) ast_type->elements[0])->base, "ptr") == 0)

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

    unsigned int from_type_kind = ir_primitive_from_ast_type(ast_from_type);
    unsigned int to_type_kind = ir_primitive_from_ast_type(ast_to_type);
    if(from_type_kind == to_type_kind && from_type_kind != TYPE_KIND_NONE) return true;

    // Mark 'to_traits' depending on the contents of 'ast_to_type'
    if(MACRO_TYPE_IS_POINTER(ast_to_type)) to_traits |= TYPE_TRAIT_POINTER;
    else if(MACRO_TYPE_IS_BASE_PTR(ast_to_type)) to_traits |= TYPE_TRAIT_BASE_PTR;
    else if(MACRO_TYPE_IS_FUNC_PTR(ast_to_type)) to_traits |= TYPE_TRAIT_FUNC_PTR;
    else if(MACRO_TYPE_IS_FIXED_ARRAY(ast_to_type)) to_traits |= TYPE_TRAIT_FIXED_ARRAY;
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
    else {
        if(from_type_kind != TYPE_KIND_NONE && from_type_kind != TYPE_KIND_FLOAT
        && from_type_kind != TYPE_KIND_DOUBLE){
            from_traits |= TYPE_TRAIT_INTEGER;
        }
    }

    if(to_type_kind == TYPE_KIND_BOOLEAN && (from_type_kind != TYPE_KIND_NONE || (from_traits & TYPE_TRAIT_BASE_PTR || from_traits & TYPE_TRAIT_POINTER))){
        ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));
        instr->id = INSTRUCTION_ISNTZERO;
        if(ir_gen_resolve_type(builder->compiler, builder->object, ast_to_type, &ir_to_type)) return FAILURE;
        instr->result_type = ir_to_type;
        instr->value = *ir_value;
        *ir_value = build_value_from_prev_instruction(builder);
        return true;
    }

    // Do bitcast if appropriate
    if( (from_traits & TYPE_TRAIT_BASE_PTR && to_traits & TYPE_TRAIT_POINTER) || // 'ptr' and '*something'
        (to_traits & TYPE_TRAIT_BASE_PTR && from_traits & TYPE_TRAIT_POINTER) || // '*something' and 'ptr'
        (from_traits & TYPE_TRAIT_FUNC_PTR && to_traits & TYPE_TRAIT_BASE_PTR) || // 'ptr' and function pointer
        (to_traits & TYPE_TRAIT_FUNC_PTR && from_traits & TYPE_TRAIT_BASE_PTR) || // function pointer and 'ptr'
        (mode & CONFORM_MODE_POINTERS && to_traits & TYPE_TRAIT_POINTER && from_traits & TYPE_TRAIT_POINTER
            && !ast_types_identical(ast_from_type, ast_to_type)) // '*something' && '*somethingelse'
        ){

        // Casting ptr -> *something
        if(ir_gen_resolve_type(builder->compiler, builder->object, ast_to_type, &ir_to_type)) return FAILURE;

        ir_instr_cast_t *bitcast_instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));
        bitcast_instr->id = INSTRUCTION_BITCAST;
        bitcast_instr->result_type = ir_to_type;
        bitcast_instr->value = *ir_value;
        *ir_value = build_value_from_prev_instruction(builder);
        return true;
    }

    // Attempt to conform integers and pointers if allowed
    if(mode & CONFORM_MODE_INTPTR){
        if(from_traits & TYPE_TRAIT_INTEGER && (to_traits & TYPE_TRAIT_POINTER
        || to_traits & TYPE_TRAIT_BASE_PTR)){
            ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));
            instr->id = INSTRUCTION_INTTOPTR;
            if(ir_gen_resolve_type(builder->compiler, builder->object, ast_to_type, &ir_to_type)) return FAILURE;
            instr->result_type = ir_to_type;
            instr->value = *ir_value;
            *ir_value = build_value_from_prev_instruction(builder);
            return true;
        }
        else if(to_traits & TYPE_TRAIT_INTEGER && (from_traits & TYPE_TRAIT_POINTER
        || from_traits & TYPE_TRAIT_BASE_PTR)){
            ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));
            instr->id = INSTRUCTION_PTRTOINT;
            if(ir_gen_resolve_type(builder->compiler, builder->object, ast_to_type, &ir_to_type)) return FAILURE;
            instr->result_type = ir_to_type;
            instr->value = *ir_value;
            *ir_value = build_value_from_prev_instruction(builder);
            return true;
        }
    }

    // Attempt to conform primitives if allowed
    if(mode & CONFORM_MODE_PRIMITIVES && from_type_kind != TYPE_KIND_NONE && to_type_kind != TYPE_KIND_NONE){
        bool from_is_float = (from_type_kind == TYPE_KIND_FLOAT || from_type_kind == TYPE_KIND_DOUBLE);
        bool to_is_float   = (to_type_kind   == TYPE_KIND_FLOAT || to_type_kind   == TYPE_KIND_DOUBLE);

        if(from_is_float == to_is_float){
            // They are either both floats or both integers
            // They are of different primitive types

            // Build instruction to cast
            ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));

            if(global_type_kind_sizes_64[from_type_kind] == global_type_kind_sizes_64[to_type_kind]){
                instr->id = INSTRUCTION_REINTERPRET; // They are the same size
                if(ir_gen_resolve_type(builder->compiler, builder->object, ast_to_type, &ir_to_type)) return FAILURE;
                instr->result_type = ir_to_type;
                instr->value = *ir_value;
                *ir_value = build_value_from_prev_instruction(builder);
                return true;
            }

            // Decide what type of cast it should be
            instr->id = global_type_kind_sizes_64[from_type_kind] < global_type_kind_sizes_64[to_type_kind] ?
                (from_is_float ? INSTRUCTION_FEXT   : INSTRUCTION_ZEXT) :
                (from_is_float ? INSTRUCTION_FTRUNC : INSTRUCTION_TRUNC);

            if(ir_gen_resolve_type(builder->compiler, builder->object, ast_to_type, &ir_to_type)) return FAILURE;
            instr->result_type = ir_to_type;
            instr->value = *ir_value;
            *ir_value = build_value_from_prev_instruction(builder);
            return true;
        }

        if(mode & CONFORM_MODE_INTFLOAT){
            // Convert int to float or float to int
            bool from_is_float = (from_type_kind == TYPE_KIND_FLOAT || from_type_kind == TYPE_KIND_DOUBLE);
            bool from_is_signed = global_type_kind_signs[from_type_kind];
            bool to_is_signed = global_type_kind_signs[to_type_kind];
            ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));

            // Decide what type of cast it should be
            instr->id = from_is_float ?
                (to_is_signed   ? INSTRUCTION_FPTOSI : INSTRUCTION_FPTOUI):
                (from_is_signed ? INSTRUCTION_SITOFP : INSTRUCTION_UITOFP);

            if(ir_gen_resolve_type(builder->compiler, builder->object, ast_to_type, &ir_to_type)) return FAILURE;
            instr->result_type = ir_to_type;
            instr->value = *ir_value;
            *ir_value = build_value_from_prev_instruction(builder);
            return true;
        }
    }

    if(mode & CONFORM_MODE_INTENUM){
        if((*ir_value)->type->kind == TYPE_KIND_U64 && to_traits & TYPE_TRAIT_INTEGER){
            // Don't bother casting the value when they are the same underlying type
            if(to_type_kind == TYPE_KIND_U64) return true;

            ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));

            // Set result type of instruction
            if(ir_gen_resolve_type(builder->compiler, builder->object, ast_to_type, &instr->result_type)) return FAILURE;

            // Set 'from' value for instruction
            instr->value = *ir_value;

            if(global_type_kind_sizes_64[from_type_kind] == global_type_kind_sizes_64[to_type_kind]){
                instr->id = INSTRUCTION_REINTERPRET; // They are the same size
                *ir_value = build_value_from_prev_instruction(builder);
                return true;
            }

            instr->id = INSTRUCTION_TRUNC;
            *ir_value = build_value_from_prev_instruction(builder);
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
