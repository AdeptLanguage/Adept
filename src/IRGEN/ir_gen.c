
#include "LEX/lex.h"
#include "UTIL/util.h"
#include "UTIL/color.h"
#include "UTIL/ground.h"
#include "UTIL/search.h"
#include "UTIL/filename.h"
#include "UTIL/builtin_type.h"
#include "BRIDGE/any.h"
#include "BRIDGE/rtti.h"
#include "IRGEN/ir_gen.h"
#include "IRGEN/ir_gen_expr.h"
#include "IRGEN/ir_gen_stmt.h"
#include "IRGEN/ir_gen_type.h"

errorcode_t ir_gen(compiler_t *compiler, object_t *object){
    ir_module_t *module = &object->ir_module;
    ast_t *ast = &object->ast;

    ir_implementation_setup();
    ir_module_init(module, ast->funcs_length, ast->globals_length);
    object->compilation_stage = COMPILATION_STAGE_IR_MODULE;

    if(ir_gen_type_mappings(compiler, object)
    || ir_gen_globals(compiler, object)
    || ir_gen_functions(compiler, object)
    || ir_gen_functions_body(compiler, object)
    || ir_gen_special_globals(compiler, object)
    || ir_gen_fill_in_rtti(compiler, object)) return FAILURE;

    return SUCCESS;
}

errorcode_t ir_gen_functions(compiler_t *compiler, object_t *object){
    // NOTE: Only ir_gens function skeletons

    ast_t *ast = &object->ast;
    ir_module_t *module = &object->ir_module;
    ast_func_t **ast_funcs = &ast->funcs;

    for(length_t ast_func_id = 0; ast_func_id != ast->funcs_length; ast_func_id++){
        ast_func_t *ast_func = &(*ast_funcs)[ast_func_id];
        if(ast_func->traits & AST_FUNC_POLYMORPHIC) continue;
        if(ir_gen_func_head(compiler, object, ast_func, ast_func_id, false, NULL)) return FAILURE;
    }

    qsort(module->func_mappings, module->func_mappings_length, sizeof(ir_func_mapping_t), ir_func_mapping_cmp);
    qsort(module->methods, module->methods_length, sizeof(ir_method_t), ir_method_cmp);
    qsort(module->generic_base_methods, module->generic_base_methods_length, sizeof(ir_generic_base_method_t), ir_generic_base_method_cmp);
    return SUCCESS;
}

errorcode_t ir_gen_func_head(compiler_t *compiler, object_t *object, ast_func_t *ast_func, length_t ast_func_id,
        bool preserve_sortedness, ir_func_mapping_t *optional_out_new_mapping){
    ir_module_t *module = &object->ir_module;

    expand((void**) &module->funcs, sizeof(ir_func_t), module->funcs_length, &module->funcs_capacity, 1, object->ast.funcs_length);
    ir_func_t *module_func = &module->funcs[module->funcs_length];
    length_t ir_func_id = module->funcs_length;

    module_func->name = ast_func->name;
    module_func->maybe_filename = NULL;
    module_func->maybe_definition_string = NULL;
    module_func->maybe_line_number = 0;
    module_func->maybe_column_number = 0;
    module_func->traits = TRAIT_NONE;
    module_func->return_type = NULL;
    module_func->argument_types = malloc(sizeof(ir_type_t*) * ast_func->arity);
    module_func->arity = 0;
    module_func->basicblocks = NULL; // Will be set after 'basicblocks' contains all of the basicblocks
    module_func->basicblocks_length = 0; // Will be set after 'basicblocks' contains all of the basicblocks
    module_func->scope = NULL;
    module_func->variable_count = 0;

    if(compiler->checks & COMPILER_NULL_CHECKS){
        module_func->maybe_definition_string = ir_gen_ast_definition_string(&module->pool, ast_func);        
        module_func->maybe_filename = compiler->objects[ast_func->source.object_index]->filename;

        int line, column;
        lex_get_location(compiler->objects[ast_func->source.object_index]->buffer, ast_func->source.index, &line, &column);
        module_func->maybe_line_number = line;
        module_func->maybe_column_number = column;
    }

    #if AST_FUNC_FOREIGN     == IR_FUNC_FOREIGN  && \
        AST_FUNC_VARARG      == IR_FUNC_VARARG   && \
        AST_FUNC_MAIN        == IR_FUNC_MAIN     && \
        AST_FUNC_STDCALL     == IR_FUNC_STDCALL  && \
        AST_FUNC_POLYMORPHIC == IR_FUNC_POLYMORPHIC
    module_func->traits = ast_func->traits & 0x1F;
    #else
    if(ast_func->traits & AST_FUNC_FOREIGN)     module_func->traits |= IR_FUNC_FOREIGN;
    if(ast_func->traits & AST_FUNC_VARARG)      module_func->traits |= IR_FUNC_VARARG;
    if(ast_func->traits & AST_FUNC_MAIN)        module_func->traits |= IR_FUNC_MAIN;
    if(ast_func->traits & AST_FUNC_STDCALL)     module_func->traits |= IR_FUNC_STDCALL;
    if(ast_func->traits & AST_FUNC_POLYMORPHIC) module_func->traits |= IR_FUNC_POLYMORPHIC;
    #endif

    expand((void**) &module->func_mappings, sizeof(ir_func_mapping_t), module->func_mappings_length, &module->func_mappings_capacity, 1, object->ast.funcs_length);
    ir_func_mapping_t *new_mapping = &module->func_mappings[module->func_mappings_length];
    new_mapping->name = ast_func->name;
    new_mapping->ir_func_id = ir_func_id;
    new_mapping->ast_func_id = ast_func_id;
    new_mapping->is_beginning_of_group = -1;
    module->func_mappings_length++;
    module->funcs_length++;

    // Give data about the created function if asked for
    if(optional_out_new_mapping) *optional_out_new_mapping = *new_mapping;

    if(preserve_sortedness){
        // SPEED: LAZY: TODO: Don't resort the entire array
        qsort(module->func_mappings, module->func_mappings_length, sizeof(ir_func_mapping_t), ir_func_mapping_cmp);

        // SPEED: LAZY: TODO: Don't invalidate the entire array
        for(size_t i = 0; i != module->func_mappings_length; i++)
            module->func_mappings[i].is_beginning_of_group = -1;
    }

    if(!(ast_func->traits & AST_FUNC_FOREIGN)){
        if(ast_func->arity > 0 && strcmp(ast_func->arg_names[0], "this") == 0){
            // This is a struct method, attach a reference to the struct
            ast_type_t *this_type = &ast_func->arg_types[0];

            // Do basic checking to make sure the type is in the format: *Structure
            if(this_type->elements_length != 2 || this_type->elements[0]->id != AST_ELEM_POINTER){
                compiler_panic(compiler, this_type->source, "Type of 'this' must be a pointer to a struct");
                return FAILURE; 
            }

            switch(this_type->elements[1]->id){
            case AST_ELEM_BASE: {
                    // Check that the base isn't a primitive
                    char *base = ((ast_elem_base_t*) this_type->elements[1])->base;
                    if(typename_is_entended_builtin_type(base)){
                        compiler_panicf(compiler, this_type->source, "Type of 'this' must be a pointer to a struct (%s is a primitive)", base);
                        return FAILURE;
                    }

                    // Find the target structure
                    ast_struct_t *target = ast_struct_find(&object->ast, ((ast_elem_base_t*) this_type->elements[1])->base);
                    if(target == NULL){
                        compiler_panicf(compiler, this_type->source, "Undeclared struct '%s'", ((ast_elem_base_t*) this_type->elements[1])->base);
                        return FAILURE;
                    }

                    // Append the method to the struct's method list
                    expand((void**) &module->methods, sizeof(ir_method_t), module->methods_length, &module->methods_capacity, 1, 4);
                    ir_method_t *method = &module->methods[module->methods_length++];
                    method->struct_name = ((ast_elem_base_t*) this_type->elements[1])->base;
                    method->name = module_func->name;
                    method->ir_func_id = ir_func_id;
                    method->ast_func_id = ast_func_id;
                    method->is_beginning_of_group = -1;

                    if(preserve_sortedness){
                        // SPEED: LAZY: TODO: Don't resort the entire array
                        qsort(module->methods, module->methods_length, sizeof(ir_method_t), ir_method_cmp);

                        // SPEED: LAZY: TODO: Don't invalidate the entire array
                        for(size_t i = 0; i != module->methods_length; i++)
                            module->methods[i].is_beginning_of_group = -1;
                    }
                }
                break;
            case AST_ELEM_GENERIC_BASE: {
                    ast_elem_generic_base_t *generic_base = (ast_elem_generic_base_t*) this_type->elements[1];
                    ast_polymorphic_struct_t *template = ast_polymorphic_struct_find(&object->ast, generic_base->name);
                    
                    if(template == NULL){
                        compiler_panicf(compiler, this_type->source, "Undeclared struct '%s'", generic_base->name);
                        return FAILURE;
                    }

                    if(template->generics_length != generic_base->generics_length){
                        compiler_panic(compiler, this_type->source, "INTERNAL ERROR: ir_gen_func_head got method with incorrect number of type parameters for generic struct type for 'this'");
                        return FAILURE;
                    }

                    // Append the method to the struct's method list
                    expand((void**) &module->generic_base_methods, sizeof(ir_generic_base_method_t), module->generic_base_methods_length, &module->generic_base_methods_capacity, 1, 4);
                    ir_generic_base_method_t *generic_base_method = &module->generic_base_methods[module->generic_base_methods_length++];
                    generic_base_method->generic_base = generic_base->name;
                    generic_base_method->generics = generic_base->generics; // NOTE: Memory for function argument types should persist, so this is ok
                    generic_base_method->generics_length = generic_base->generics_length;
                    generic_base_method->name = module_func->name;
                    generic_base_method->ir_func_id = ir_func_id;
                    generic_base_method->ast_func_id = ast_func_id;
                    generic_base_method->is_beginning_of_group = -1;

                    if(preserve_sortedness){
                        // SPEED: LAZY: TODO: Don't resort the entire array
                        qsort(module->generic_base_methods, module->generic_base_methods_length, sizeof(ir_generic_base_method_t), ir_generic_base_method_cmp);

                        // SPEED: LAZY: TODO: Don't invalidate the entire array
                        for(size_t i = 0; i != module->generic_base_methods_length; i++)
                            module->generic_base_methods[i].is_beginning_of_group = -1;
                    }
                }
                break;
            default:
                compiler_panic(compiler, this_type->source, "Type of 'this' must be a pointer to a struct");
                return FAILURE;
            }
        }
    } else {
        while(module_func->arity != ast_func->arity){
            if(ir_gen_resolve_type(compiler, object, &ast_func->arg_types[module_func->arity], &module_func->argument_types[module_func->arity])) return FAILURE;
            module_func->arity++;
        }
    }

    if(ast_func->traits & AST_FUNC_MAIN && ast_type_is_void(&ast_func->return_type)){
        // If it's the main function and returns void, return int under the hood
        module_func->return_type = ir_pool_alloc(&module->pool, sizeof(ir_type_t));
        module_func->return_type->kind = TYPE_KIND_S32;
        // neglect 'module_func->return_type->extra'
    } else {
        if(ir_gen_resolve_type(compiler, object, &ast_func->return_type, &module_func->return_type)) return FAILURE;
    }

    return SUCCESS;
}

errorcode_t ir_gen_functions_body(compiler_t *compiler, object_t *object){
    // NOTE: Only ir_gens function body; assumes skeleton already exists

    ast_func_t **ast_funcs = &object->ast.funcs;
    length_t *mappings_length = &object->ir_module.func_mappings_length;

    // Create jobs for already existing function mappings
    ir_jobs_t jobs;
    jobs.jobs = malloc(sizeof(ir_func_mapping_t) * (*mappings_length + 4));
    jobs.length = *mappings_length;
    jobs.capacity = *mappings_length + 4;
    memcpy(jobs.jobs, object->ir_module.func_mappings, sizeof(ir_func_mapping_t) * *mappings_length);

    while(jobs.length != 0){
        ir_func_mapping_t *job = &jobs.jobs[--jobs.length];
        if((*ast_funcs)[job->ast_func_id].traits & AST_FUNC_FOREIGN) continue;

        if(ir_gen_func_statements(compiler, object, job->ast_func_id, job->ir_func_id, &jobs)){
            free(jobs.jobs);
            return FAILURE;
        }
    }

    free(jobs.jobs);
    return SUCCESS;
}

errorcode_t ir_gen_globals(compiler_t *compiler, object_t *object){
    ast_t *ast = &object->ast;
    ir_module_t *module = &object->ir_module;

    for(length_t g = 0; g != ast->globals_length; g++){
        ir_global_t *global = &module->globals[g];

        global->name = ast->globals[g].name;
        global->traits = ast->globals[g].traits & AST_GLOBAL_EXTERNAL ? IR_GLOBAL_EXTERNAL : TRAIT_NONE;
        global->trusted_static_initializer = NULL;

        if(ir_gen_resolve_type(compiler, object, &ast->globals[g].type, &global->type)){
            return FAILURE;
        }

        module->globals_length++;
    }

    return SUCCESS;
}

errorcode_t ir_gen_globals_init(ir_builder_t *builder){
    // Generates instructions for initializing global variables
    ast_global_t *globals = builder->object->ast.globals;
    length_t globals_length = builder->object->ast.globals_length;

    for(length_t g = 0; g != globals_length; g++){
        ast_global_t *ast_global = &globals[g];
        if(ast_global->initial == NULL) continue;

        ir_value_t *value;
        ast_type_t value_ast_type;

        if(ir_gen_expression(builder, ast_global->initial, &value, false, &value_ast_type)) return FAILURE;

        if(!ast_types_conform(builder, &value, &value_ast_type, &ast_global->type, CONFORM_MODE_ASSIGNING)){
            char *a_type_str = ast_type_str(&value_ast_type);
            char *b_type_str = ast_type_str(&ast_global->type);
            compiler_panicf(builder->compiler, ast_global->initial->source, "Incompatible types '%s' and '%s'", a_type_str, b_type_str);
            free(a_type_str);
            free(b_type_str);
            ast_type_free(&value_ast_type);
            return FAILURE;
        }

        ast_type_free(&value_ast_type);

        ir_type_t *ptr_to_type = ir_type_pointer_to(builder->pool, builder->object->ir_module.globals[g].type);
        ir_value_t *destination = build_gvarptr(builder, ptr_to_type, g);
        build_store(builder, value, destination, ast_global->source);
    }
    return SUCCESS;
}

errorcode_t ir_gen_special_globals(compiler_t *compiler, object_t *object){
    ast_global_t *globals = object->ast.globals;
    length_t globals_length = object->ast.globals_length;

    for(length_t g = 0; g != globals_length; g++){
        ast_global_t *ast_global = &globals[g];
        if(!(ast_global->traits & AST_GLOBAL_SPECIAL)) continue;
        if(ir_gen_special_global(compiler, object, ast_global, g)) return FAILURE;
    }

    return SUCCESS;
}

errorcode_t ir_gen_special_global(compiler_t *compiler, object_t *object, ast_global_t *ast_global, length_t global_variable_id){
    // NOTE: Assumes (ast_global->traits & AST_GLOBAL_SPECIAL)

    ir_module_t *ir_module = &object->ir_module;
    ir_pool_t *pool = &ir_module->pool;
    type_table_t *type_table = object->ast.type_table;

    ir_type_t *ptr_to_type = ir_pool_alloc(pool, sizeof(ir_type_t));
    ptr_to_type->kind = TYPE_KIND_POINTER;
    ptr_to_type->extra = NULL;

    if(ir_gen_resolve_type(compiler, object, &ast_global->type, (ir_type_t**) &ptr_to_type->extra)){
        redprintf("INTERNAL ERROR: Failed to get IR type for special global variable\n");
        return FAILURE;
    }

    // NOTE: DANGEROUS: 'global_variable_id' is assumed to be the same between AST and IR global variable lists
    ir_global_t *ir_global = &ir_module->globals[global_variable_id];

    if(ast_global->traits & AST_GLOBAL___TYPES__){
        ir_type_t *any_type_type, *any_struct_type_type, *any_ptr_type_type,
            *any_type_ptr_type = NULL, *ubyte_ptr_type;
        
        if(compiler->traits & COMPILER_NO_TYPEINFO){
            if(!ir_type_map_find(&ir_module->type_map, "AnyType", &any_type_type)){
                redprintf("INTERNAL ERROR: Failed to get 'AnyType' which should've been injected\n");
                redprintf("    (when creating null pointer to initialize __types__ because type info was disabled)\n")
                return FAILURE;
            }

            any_type_ptr_type = ir_type_pointer_to(pool, any_type_type);
            ir_global->trusted_static_initializer = build_null_pointer_of_type(pool, ir_type_pointer_to(pool, any_type_ptr_type));
            return SUCCESS;
        }

        if(!ir_type_map_find(&ir_module->type_map, "AnyType", &any_type_type)
        || !ir_type_map_find(&ir_module->type_map, "AnyStructType", &any_struct_type_type)
        || !ir_type_map_find(&ir_module->type_map, "AnyPtrType", &any_ptr_type_type)
        || !ir_type_map_find(&ir_module->type_map, "ubyte", &ubyte_ptr_type)){
            redprintf("INTERNAL ERROR: Failed to find types used by the runtime type table that should've been injected\n");
            return FAILURE;
        }

        ir_type_t *usize_type = ir_pool_alloc(pool, sizeof(ir_type_t));
        usize_type->kind = TYPE_KIND_U64;

        any_type_ptr_type = ir_type_pointer_to(pool, any_type_type);
        ubyte_ptr_type = ir_type_pointer_to(pool, ubyte_ptr_type);

        ir_value_array_literal_t *array_literal = ir_pool_alloc(pool, sizeof(ir_value_array_literal_t));
        array_literal->values = NULL; // Will be set to array_values
        array_literal->length = type_table->length;

        ir_value_t *array_value = ir_pool_alloc(pool, sizeof(ir_value_t));
        array_value->value_type = VALUE_TYPE_ARRAY_LITERAL;
        array_value->type = ir_type_pointer_to(pool, any_type_ptr_type);
        array_value->extra = array_literal;
        
        ir_value_t **array_values = ir_pool_alloc(pool, sizeof(ir_value_t*) * type_table->length);

        for(length_t i = 0; i != type_table->length; i++){
            if(ir_gen_resolve_type(compiler, object, &type_table->entries[i].ast_type, &type_table->entries[i].ir_type)){
                return FAILURE;
            }

            switch(type_table->entries[i].ir_type->kind){
            case TYPE_KIND_POINTER:
                array_values[i] = build_anon_global(ir_module, any_ptr_type_type, true);
                break;
            case TYPE_KIND_STRUCTURE:
                array_values[i] = build_anon_global(ir_module, any_struct_type_type, true);
                break;
            default:
                array_values[i] = build_anon_global(ir_module, any_type_type, true);
            }
        }

        for(length_t i = 0; i != type_table->length; i++){
            ir_type_t *initializer_type;
            ir_value_t **initializer_members;
            length_t initializer_members_length;
            unsigned int any_type_kind_id = ANY_TYPE_KIND_VOID;
            unsigned int type_type_kind = type_table->entries[i].ir_type->kind;

            switch(type_type_kind){
            case TYPE_KIND_POINTER: {
                    /* struct AnyPtrType(kind AnyTypeKind, name *ubyte, is_alias bool, subtype *AnyType) */

                    initializer_members = ir_pool_alloc(pool, sizeof(ir_value_t*) * 4);
                    initializer_members_length = 4;

                    maybe_index_t subtype_index = -1;

                    // HACK: We really shouldn't be doing this
                    if(type_table->entries[i].ast_type.elements_length > 1){
                        ast_type_t dereferenced = ast_type_clone(&type_table->entries[i].ast_type);
                        
                        ast_type_dereference(&dereferenced);

                        char *dereferenced_name = ast_type_str(&dereferenced);
                        subtype_index = type_table_find(type_table, dereferenced_name);
                        ast_type_free(&dereferenced);
                        free(dereferenced_name);
                    }

                    if(subtype_index == -1){
                        ir_value_t *null_pointer = build_null_pointer_of_type(pool, any_type_ptr_type);
                        initializer_members[3] = null_pointer; // subtype
                    } else {
                        initializer_members[3] = build_const_bitcast(pool, array_values[subtype_index], any_type_ptr_type); // subtype
                    }

                    initializer_type = any_ptr_type_type;
                }
                break;
            case TYPE_KIND_STRUCTURE: {
                    /* struct AnyStructType (kind AnyTypeKind, name *ubyte, members **AnyType, length usize, offsets *usize, member_names **ubyte, is_packed bool) */

                    ir_type_extra_composite_t *composite = (ir_type_extra_composite_t*) type_table->entries[i].ir_type->extra;
                    initializer_members = ir_pool_alloc(pool, sizeof(ir_value_t*) * 8);
                    initializer_members_length = 8;

                    ir_value_t **composite_members = ir_pool_alloc(pool, sizeof(ir_value_t*) * composite->subtypes_length);
                    ir_value_t **composite_offsets = ir_pool_alloc(pool, sizeof(ir_value_t*) * composite->subtypes_length);
                    ir_value_t **composite_member_names = ir_pool_alloc(pool, sizeof(ir_value_t*) * composite->subtypes_length);

                    ast_elem_t *elem = type_table->entries[i].ast_type.elements[0];

                    if(elem->id == AST_ELEM_GENERIC_BASE){
                        ast_elem_generic_base_t *generic_base = (ast_elem_generic_base_t*) elem;
                        
                        // Find polymorphic struct
                        ast_polymorphic_struct_t *template = ast_polymorphic_struct_find(&object->ast, generic_base->name);
                        
                        if(template == NULL){
                            redprintf("INTERNAL ERROR: Failed to find polymorphic struct '%s' that should exist when generating runtime type table!\n", generic_base->name);
                            return FAILURE;
                        }

                        // Substitute generic type parameters
                        ast_type_var_catalog_t catalog;
                        ast_type_var_catalog_init(&catalog);

                        if(template->generics_length != generic_base->generics_length){
                            redprintf("INTERNAL ERROR: Polymorphic struct '%s' type parameter length mismatch when generating runtime type table!\n", generic_base->name);
                            ast_type_var_catalog_free(&catalog);
                            return FAILURE;
                        }

                        for(length_t i = 0; i != template->generics_length; i++){
                            ast_type_var_catalog_add(&catalog, template->generics[i], &generic_base->generics[i]);
                        }

                        ast_type_t container_ast_type;
                        if(resolve_type_polymorphics(compiler, type_table, &catalog, &type_table->entries[i].ast_type, &container_ast_type)){
                            ast_type_var_catalog_free(&catalog);
                            return FAILURE;
                        }

                        // Generate meta data
                        for(length_t s = 0; s != composite->subtypes_length; s++){
                            ast_type_t member_ast_type;

                            // DANGEROUS: WARNING: Accessing 'template->field_types' based on index going to 'composite->subtypes_length'
                            if(resolve_type_polymorphics(compiler, type_table, &catalog, &template->field_types[s], &member_ast_type)){
                                ast_type_var_catalog_free(&catalog);
                                return FAILURE;
                            }
                            
                            char *member_type_name = ast_type_str(&member_ast_type);
                            maybe_index_t subtype_index = type_table_find(type_table, member_type_name);
                            free(member_type_name);

                            if(subtype_index == -1){
                                composite_members[s] = build_null_pointer_of_type(pool, any_type_ptr_type); // members[s]
                            } else {
                                composite_members[s] = build_const_bitcast(pool, array_values[subtype_index], any_type_ptr_type); // members[s]
                            }

                            ir_type_t *struct_ir_type;
                            if(ir_gen_resolve_type(compiler, object, &container_ast_type, &struct_ir_type)){
                                redprintf("INTERNAL ERROR: ir_gen_resolve_type for RTTI composite offset computation failed!\n");
                                ast_type_free(&container_ast_type);
                                ast_type_free(&member_ast_type);
                                return FAILURE;
                            }

                            composite_offsets[s] = build_offsetof_ex(pool, usize_type, struct_ir_type, s);;
                            composite_member_names[s] = build_literal_cstr_ex(pool, &ir_module->type_map, template->field_names[s]);
                            ast_type_free(&member_ast_type);
                        }

                        ast_type_free(&container_ast_type);
                        ast_type_var_catalog_free(&catalog);
                    } else if(elem->id == AST_ELEM_BASE){
                        const char *struct_name = ((ast_elem_base_t*) elem)->base;
                        ast_struct_t *structure = ast_struct_find(&object->ast, struct_name);

                        if(structure == NULL){
                            redprintf("INTERNAL ERROR: Failed to find struct '%s' that should exist when generating runtime type table!\n", struct_name);
                            return FAILURE;
                        }

                        if(structure->field_count != composite->subtypes_length){
                            redprintf("INTERNAL ERROR: Mismatching member count of IR as AST types for struct '%s' when generating runtime type table!\n", struct_name);
                            return FAILURE;
                        }

                        for(length_t s = 0; s != composite->subtypes_length; s++){
                            char *member_type_name = ast_type_str(&structure->field_types[s]);
                            maybe_index_t subtype_index = type_table_find(type_table, member_type_name);
                            free(member_type_name);

                            if(subtype_index == -1){
                                composite_members[s] = build_null_pointer_of_type(pool, any_type_ptr_type); // members[s]
                            } else {
                                composite_members[s] = build_const_bitcast(pool, array_values[subtype_index], any_type_ptr_type); // members[s]
                            }

                            // SPEED: TODO: Make 'struct_ast_type' not dynamically allocated
                            ast_type_t struct_ast_type;
                            ast_type_make_base(&struct_ast_type, strclone(struct_name));

                            ir_type_t *struct_ir_type;
                            if(ir_gen_resolve_type(compiler, object, &struct_ast_type, &struct_ir_type)){
                                redprintf("INTERNAL ERROR: ir_gen_resolve_type for RTTI composite offset computation failed!\n");
                                ast_type_free(&struct_ast_type);
                                return FAILURE;
                            }

                            ast_type_free(&struct_ast_type);

                            composite_offsets[s] = build_offsetof_ex(pool, usize_type, struct_ir_type, s);
                            composite_member_names[s] = build_literal_cstr_ex(pool, &ir_module->type_map, structure->field_names[s]);
                        }
                    } else {
                        redprintf("INTERNAL ERROR: Unknown AST type element for TYPE_KIND_STRUCTURE when generating runtime type information!\n");
                        return FAILURE;
                    }

                    ir_value_t *members_array = build_static_array(pool, any_type_ptr_type, composite_members, composite->subtypes_length);
                    ir_value_t *offsets_array = build_static_array(pool, usize_type, composite_offsets, composite->subtypes_length);
                    ir_value_t *member_names_array = build_static_array(pool, ubyte_ptr_type, composite_member_names, composite->subtypes_length);

                    initializer_members[3] = members_array;
                    initializer_members[4] = build_literal_usize(pool, composite->subtypes_length); // length
                    initializer_members[5] = offsets_array;
                    initializer_members[6] = member_names_array;
                    initializer_members[7] = build_bool(pool, composite->traits & TYPE_KIND_COMPOSITE_PACKED); // is_packed
                    initializer_type = any_struct_type_type;
                }
                break;
            default:
                initializer_members = ir_pool_alloc(pool, sizeof(ir_value_t*) * 3);
                initializer_members_length = 3;
                initializer_type = any_type_type;
            }

            switch(type_type_kind){
            case TYPE_KIND_NONE:        any_type_kind_id = ANY_TYPE_KIND_VOID; break;
            case TYPE_KIND_POINTER:     any_type_kind_id = ANY_TYPE_KIND_PTR; break;
            case TYPE_KIND_S8:          any_type_kind_id = ANY_TYPE_KIND_BYTE; break;
            case TYPE_KIND_S16:         any_type_kind_id = ANY_TYPE_KIND_SHORT; break;
            case TYPE_KIND_S32:         any_type_kind_id = ANY_TYPE_KIND_INT; break;
            case TYPE_KIND_S64:         any_type_kind_id = ANY_TYPE_KIND_LONG; break;
            case TYPE_KIND_U8:          any_type_kind_id = ANY_TYPE_KIND_UBYTE; break;
            case TYPE_KIND_U16:         any_type_kind_id = ANY_TYPE_KIND_USHORT; break;
            case TYPE_KIND_U32:         any_type_kind_id = ANY_TYPE_KIND_UINT; break;
            case TYPE_KIND_U64:         any_type_kind_id = ANY_TYPE_KIND_ULONG; break;
            case TYPE_KIND_FLOAT:       any_type_kind_id = ANY_TYPE_KIND_FLOAT; break;
            case TYPE_KIND_DOUBLE:      any_type_kind_id = ANY_TYPE_KIND_DOUBLE; break;
            case TYPE_KIND_BOOLEAN:     any_type_kind_id = ANY_TYPE_KIND_BOOL; break;
            case TYPE_KIND_STRUCTURE:   any_type_kind_id = ANY_TYPE_KIND_STRUCT; break;
            case TYPE_KIND_FUNCPTR:     any_type_kind_id = ANY_TYPE_KIND_FUNC_PTR; break;
            case TYPE_KIND_FIXED_ARRAY: any_type_kind_id = ANY_TYPE_KIND_FIXED_ARRAY; break;

            // Unsupported Type Kinds
            case TYPE_KIND_HALF:        any_type_kind_id = ANY_TYPE_KIND_USHORT; break;
            // case TYPE_KIND_UNION: ignored
            }

            initializer_members[0] = build_literal_usize(pool, any_type_kind_id); // kind
            initializer_members[1] = build_literal_cstr_ex(pool, &ir_module->type_map, type_table->entries[i].name); // name
            initializer_members[2] = build_bool(pool, type_table->entries[i].is_alias); // is_alias

            ir_value_t *initializer = build_static_struct(ir_module, initializer_type, initializer_members, initializer_members_length, false);
            build_anon_global_initializer(ir_module, array_values[i], initializer);

            if(initializer_type != any_type_ptr_type){
                array_values[i] = build_const_bitcast(pool, array_values[i], any_type_ptr_type);
            }
        }

        array_literal->values = array_values;
        ir_global->trusted_static_initializer = array_value;
        return SUCCESS;
    }

    if(ast_global->traits & AST_GLOBAL___TYPES_LENGTH__){
        if(compiler->traits & COMPILER_NO_TYPEINFO){
            ir_global->trusted_static_initializer = build_literal_usize(pool, 0);
            return SUCCESS;
        }

        ir_value_t *value = build_literal_usize(pool, type_table->length);
        ir_global->trusted_static_initializer = value;
        return SUCCESS;
    }

    if(ast_global->traits & AST_GLOBAL___TYPE_KINDS__){
        ir_type_t *ubyte_ptr_type, *ubyte_ptr_ptr_type;

        if(!ir_type_map_find(&ir_module->type_map, "ubyte", &ubyte_ptr_type)){
            redprintf("INTERNAL ERROR: Failed to find types used by the runtime type table that should've been injected\n");
            return FAILURE;
        }

        // Construct IR Types we need
        ubyte_ptr_type = ir_type_pointer_to(pool, ubyte_ptr_type);
        ubyte_ptr_ptr_type = ir_type_pointer_to(pool, ubyte_ptr_type);

        if(compiler->traits & COMPILER_NO_TYPEINFO){
            ir_global->trusted_static_initializer = build_null_pointer_of_type(pool, ubyte_ptr_ptr_type);
            return SUCCESS;
        }

        ir_value_array_literal_t *kinds_array_literal = ir_pool_alloc(pool, sizeof(ir_value_array_literal_t));
        kinds_array_literal->values = NULL; // Will be set to array_values
        kinds_array_literal->length = MAX_ANY_TYPE_KIND + 1;

        ir_value_t *kinds_array_value = ir_pool_alloc(pool, sizeof(ir_value_t));
        kinds_array_value->value_type = VALUE_TYPE_ARRAY_LITERAL;
        kinds_array_value->type = ir_type_pointer_to(pool, ubyte_ptr_type);
        kinds_array_value->extra = kinds_array_literal;
        
        ir_value_t **array_values = ir_pool_alloc(pool, sizeof(ir_value_t*) * (MAX_ANY_TYPE_KIND + 1));

        for(length_t i = 0; i != MAX_ANY_TYPE_KIND + 1; i++){
            array_values[i] = build_literal_cstr_ex(pool, &ir_module->type_map, (weak_cstr_t) any_type_kind_names[i]);
        }

        kinds_array_literal->values = array_values;
        ir_global->trusted_static_initializer = kinds_array_value;
        return SUCCESS;
    }

    if(ast_global->traits & AST_GLOBAL___TYPE_KINDS_LENGTH__){
        if(compiler->traits & COMPILER_NO_TYPEINFO){
            ir_global->trusted_static_initializer = build_literal_usize(pool, 0);
            return SUCCESS;
        }

        ir_value_t *value = build_literal_usize(pool, MAX_ANY_TYPE_KIND + 1);
        ir_global->trusted_static_initializer = value;
        return SUCCESS;
    }

    // Should never reach
    redprintf("INTERNAL ERROR: Encountered unknown special global variable '%s'!\n", ast_global->name);
    return FAILURE;
}

errorcode_t ir_gen_fill_in_rtti(compiler_t *compiler, object_t *object){
    ir_module_t *ir_module = &object->ir_module;

    type_table_t *type_table = object->ast.type_table;
    if(type_table == NULL) return FAILURE;

    rtti_relocation_t *relocations = ir_module->rtti_relocations;
    length_t relocations_length = ir_module->rtti_relocations_length;

    for(length_t i = 0; i != relocations_length; i++){
        if(rtti_resolve(compiler, type_table, &relocations[i])) return FAILURE;
    }

    return SUCCESS;
}

const char *ir_gen_ast_definition_string(ir_pool_t *pool, ast_func_t *ast_func){
    length_t length = 0;
    length_t capacity = 96;
    strong_cstr_t string = malloc(capacity);

    // TODO: CLEANUP: Clean up this messy code
    // TODO: CLEANUP: Factor out a lot of this garbage

    {
        // Append function name
        length_t name_length = strlen(ast_func->name);
        expand((void**) &string, sizeof(char), length, &capacity, name_length, capacity);
        memcpy(&string[length], ast_func->name, name_length);
        length += name_length;
    }

    // Append '('
    expand((void**) &string, sizeof(char), length, &capacity, 1, capacity);
    string[length++] = '(';

    // Append argument list
    for(length_t i = 0; i != ast_func->arity; i++){
        char *type_str = ast_type_str(&ast_func->arg_types[i]);
        length_t type_str_len = strlen(type_str);
        expand((void**) &string, sizeof(char), length, &capacity, type_str_len, capacity);
        memcpy(&string[length], type_str, type_str_len);
        length += type_str_len;
        free(type_str);

        if(i + 1 != ast_func->arity){
            expand((void**) &string, sizeof(char), length, &capacity, 2, capacity);
            string[length++] = ',';
            string[length++] = ' ';
        }
    }

    // Append '( '
    expand((void**) &string, sizeof(char), length, &capacity, 2, capacity);
    string[length++] = ')';
    string[length++] = ' ';

    {
        // Append return type
        char *type_str = ast_type_str(&ast_func->return_type);
        length_t type_str_len = strlen(type_str);
        expand((void**) &string, sizeof(char), length, &capacity, type_str_len, capacity);
        memcpy(&string[length], type_str, type_str_len);
        length += type_str_len;
        free(type_str);
    }

    // Terminate string
    expand((void**) &string, sizeof(char), length, &capacity, 1, capacity);
    string[length++] = '\0';

    // Trade heap allocated string for IR memory pooled string
    char *destination = ir_pool_alloc(pool, length);
    memcpy(destination, string, length);
    free(string);
    return destination;
}

int ir_func_mapping_cmp(const void *a, const void *b){
    int diff = strcmp(((ir_func_mapping_t*) a)->name, ((ir_func_mapping_t*) b)->name);
    if(diff != 0) return diff;
    return (int) ((ir_func_mapping_t*) a)->ast_func_id - (int) ((ir_func_mapping_t*) b)->ast_func_id;
}

int ir_method_cmp(const void *a, const void *b){
    int diff = strcmp(((ir_method_t*) a)->struct_name, ((ir_method_t*) b)->struct_name);
    if(diff != 0) return diff;
    diff = strcmp(((ir_method_t*) a)->name, ((ir_method_t*) b)->name);
    if(diff != 0) return diff;
    return (int) ((ir_method_t*) a)->ast_func_id - (int) ((ir_method_t*) b)->ast_func_id;
}

int ir_generic_base_method_cmp(const void *a, const void *b){
    int diff = strcmp(((ir_generic_base_method_t*) a)->generic_base, ((ir_generic_base_method_t*) b)->generic_base);
    if(diff != 0) return diff;
    diff = strcmp(((ir_generic_base_method_t*) a)->name, ((ir_generic_base_method_t*) b)->name);
    if(diff != 0) return diff;
    return (int) ((ir_generic_base_method_t*) a)->ast_func_id - (int) ((ir_generic_base_method_t*) b)->ast_func_id;
}
