
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
#include "IRGEN/ir_gen_find.h"
#include "IRGEN/ir_gen_stmt.h"
#include "IRGEN/ir_gen_type.h"
#include "IRGEN/ir_gen_rtti.h"

errorcode_t ir_gen(compiler_t *compiler, object_t *object){
    ir_module_t *module = &object->ir_module;
    ast_t *ast = &object->ast;

    ir_job_list_t job_list;
    memset(&job_list, 0, sizeof(ir_job_list_t));

    ir_implementation_setup();
    ir_module_init(module, ast->funcs_length, ast->globals_length, ast->funcs_length + ast->func_aliases_length + 32);
    object->compilation_stage = COMPILATION_STAGE_IR_MODULE;

    if(ir_gen_type_mappings(compiler, object)
    || ir_gen_globals(compiler, object)
    || ir_gen_functions(compiler, object, &job_list)
    || ir_gen_functions_body(compiler, object, &job_list)
    || ir_gen_special_globals(compiler, object)
    || ir_gen_fill_in_rtti(object)){
        free(job_list.jobs);
        return FAILURE;
    }

    return SUCCESS;
}

errorcode_t ir_gen_functions(compiler_t *compiler, object_t *object, ir_job_list_t *job_list){
    // NOTE: Only ir_gens function skeletons

    ast_t *ast = &object->ast;
    ir_module_t *module = &object->ir_module;
    ast_func_t **ast_funcs = &ast->funcs;
    ast_func_alias_t **ast_func_aliases = &ast->func_aliases;

    // Setup IR variadic array type if it exists
    if(ast->common.ast_variadic_array){
        // Resolve ast_variadic_array type
        if(ir_gen_resolve_type(compiler, object, ast->common.ast_variadic_array, &module->common.ir_variadic_array)) return FAILURE;
    }

    // Generate function skeletons
    for(length_t ast_func_id = 0; ast_func_id != ast->funcs_length; ast_func_id++){
        ast_func_t *ast_func = &(*ast_funcs)[ast_func_id];
        if(ast_func->traits & AST_FUNC_POLYMORPHIC) continue;
        if(ir_gen_func_head(compiler, object, ast_func, ast_func_id, false, NULL)) return FAILURE;
    }

    // Sort various mappings
    qsort(module->func_mappings, module->func_mappings_length, sizeof(ir_func_mapping_t), ir_func_mapping_cmp);
    qsort(module->methods, module->methods_length, sizeof(ir_method_t), ir_method_cmp);
    qsort(module->generic_base_methods, module->generic_base_methods_length, sizeof(ir_generic_base_method_t), ir_generic_base_method_cmp);

    if(ir_gen_job_list(object, job_list)) return FAILURE;

    // Generate function aliases
    trait_t req_traits_mask = AST_FUNC_VARARG | AST_FUNC_VARIADIC;
    for(length_t ast_func_alias_id = 0; ast_func_alias_id != ast->func_aliases_length; ast_func_alias_id++){
        ast_func_alias_t *falias = &(*ast_func_aliases)[ast_func_alias_id];

        funcpair_t pair;
        errorcode_t error;
        bool is_unique = true;

        if(falias->match_first_of_name){
            error = ir_gen_find_func_named(object, falias->to, &is_unique, &pair);
        } else {
            error = ir_gen_find_func(compiler, object, job_list, falias->to, falias->arg_types, falias->arity, req_traits_mask, falias->required_traits, &pair);
        }

        if(error){
            compiler_panicf(compiler, falias->source, "Failed to find proper destination function '%s'", falias->to);
            return FAILURE;
        }

        if(!is_unique && compiler_warnf(compiler, falias->source, "Multiple functions named '%s', using the first of them", falias->to)){
            return FAILURE;
        }
        
        ir_module_insert_func_mapping(module, falias->from, pair.ir_func_id, pair.ast_func_id, true);
    }

    errorcode_t error;

    // Find __variadic_array__ (if it exists)
    error = ir_gen_find_special_func(compiler, object, "__variadic_array__", &module->common.variadic_ir_func_id);
    if(error == ALT_FAILURE) return FAILURE;
    
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
    module_func->argument_types = malloc(sizeof(ir_type_t*) * (ast_func->traits & AST_FUNC_VARIADIC ? ast_func->arity + 1 : ast_func->arity));
    module_func->arity = 0;
    module_func->basicblocks = NULL; // Will be set after 'basicblocks' contains all of the basicblocks
    module_func->basicblocks_length = 0; // Will be set after 'basicblocks' contains all of the basicblocks
    module_func->scope = NULL;
    module_func->variable_count = 0;
    module_func->export_as = ast_func->export_as;

    if(ast_func->traits & AST_FUNC_VARIADIC){
        module_func->argument_types[ast_func->arity] = module->common.ir_variadic_array;
    }

    if(compiler->checks & COMPILER_NULL_CHECKS){
        module_func->maybe_definition_string = ir_gen_ast_definition_string(&module->pool, ast_func);        
        module_func->maybe_filename = compiler->objects[ast_func->source.object_index]->filename;

        int line, column;
        lex_get_location(compiler->objects[ast_func->source.object_index]->buffer, ast_func->source.index, &line, &column);
        module_func->maybe_line_number = line;
        module_func->maybe_column_number = column;
    }

    #if AST_FUNC_FOREIGN     == IR_FUNC_FOREIGN      && \
        AST_FUNC_VARARG      == IR_FUNC_VARARG       && \
        AST_FUNC_MAIN        == IR_FUNC_MAIN         && \
        AST_FUNC_STDCALL     == IR_FUNC_STDCALL      && \
        AST_FUNC_POLYMORPHIC == IR_FUNC_POLYMORPHIC
    module_func->traits = ast_func->traits & 0x1F;
    #else
    if(ast_func->traits & AST_FUNC_FOREIGN)     module_func->traits |= IR_FUNC_FOREIGN;
    if(ast_func->traits & AST_FUNC_VARARG)      module_func->traits |= IR_FUNC_VARARG;
    if(ast_func->traits & AST_FUNC_MAIN)        module_func->traits |= IR_FUNC_MAIN;
    if(ast_func->traits & AST_FUNC_STDCALL)     module_func->traits |= IR_FUNC_STDCALL;
    if(ast_func->traits & AST_FUNC_POLYMORPHIC) module_func->traits |= IR_FUNC_POLYMORPHIC;
    #endif

    module->funcs_length++;

    ir_func_mapping_t *new_mapping = ir_module_insert_func_mapping(module, ast_func->name, ir_func_id, ast_func_id, preserve_sortedness);
    if(optional_out_new_mapping) *optional_out_new_mapping = *new_mapping;

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
                    ast_composite_t *target = ast_composite_find_exact(&object->ast, ((ast_elem_base_t*) this_type->elements[1])->base);

                    if(target == NULL){
                        compiler_panicf(compiler, this_type->source, "Undeclared struct '%s'", ((ast_elem_base_t*) this_type->elements[1])->base, NULL);
                        return FAILURE;
                    }

                    const weak_cstr_t method_name = module_func->name;
                    weak_cstr_t struct_name = ((ast_elem_base_t*) this_type->elements[1])->base;
                    ir_module_insert_method(module, struct_name, method_name, ir_func_id, ast_func_id, preserve_sortedness);
                }
                break;
            case AST_ELEM_GENERIC_BASE: {
                    ast_elem_generic_base_t *generic_base = (ast_elem_generic_base_t*) this_type->elements[1];
                    ast_polymorphic_composite_t *template = ast_polymorphic_composite_find_exact(&object->ast, generic_base->name);
                    
                    if(template == NULL){
                        compiler_panicf(compiler, this_type->source, "Undeclared polymorphic struct '%s'", generic_base->name);
                        return FAILURE;
                    }

                    if(template->generics_length != generic_base->generics_length){
                        compiler_panic(compiler, this_type->source, "INTERNAL ERROR: ir_gen_func_head got method with incorrect number of type parameters for generic struct type for 'this'");
                        return FAILURE;
                    }

                    ir_module_insert_generic_method(module,
                        generic_base->name,
                        generic_base->generics, // NOTE: Memory for function argument types should persist, so this is ok
                        generic_base->generics_length,
                        module_func->name,
                        ir_func_id,
                        ast_func_id,
                    preserve_sortedness);
                }
                break;
            default:
                compiler_panic(compiler, this_type->source, "Type of 'this' must be a pointer to a struct");
                return FAILURE;
            }
        }
    } else {
        // If 'foreign' function, we don't ever process the statements, which is where we generate the IR argument types here instead

        while(module_func->arity != ast_func->arity){
            if(ir_gen_resolve_type(compiler, object, &ast_func->arg_types[module_func->arity], &module_func->argument_types[module_func->arity])) return FAILURE;
            module_func->arity++;
        }
    }

    if(ast_func->traits & AST_FUNC_MAIN){
        module->common.has_main = true;
        module->common.ast_main_id = ast_func_id;
        module->common.ir_main_id = ir_func_id;
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

errorcode_t ir_gen_functions_body(compiler_t *compiler, object_t *object, ir_job_list_t *job_list){
    // NOTE: Only ir_gens function body; assumes skeleton already exists

    ast_func_t **ast_funcs = &object->ast.funcs;
    
    object->ir_module.init_builder = malloc(sizeof(ir_builder_t));
    object->ir_module.deinit_builder = malloc(sizeof(ir_builder_t));
    ir_builder_init(object->ir_module.init_builder, compiler, object, object->ir_module.common.ast_main_id, object->ir_module.common.ir_main_id, job_list, true);
    ir_builder_init(object->ir_module.deinit_builder, compiler, object, object->ir_module.common.ast_main_id, object->ir_module.common.ir_main_id, job_list, true);

    while(job_list->length != 0){
        ir_func_mapping_t *job = &job_list->jobs[--job_list->length];
        if((*ast_funcs)[job->ast_func_id].traits & AST_FUNC_FOREIGN) continue;

        if(ir_gen_functions_body_statements(compiler, object, job->ast_func_id, job->ir_func_id, job_list)){
            return FAILURE;
        }
    }
    
    return SUCCESS;
}

errorcode_t ir_gen_functions_body_statements(compiler_t *compiler, object_t *object, length_t ast_func_id, length_t ir_func_id, ir_job_list_t *job_list){
    // ir_gens statements into basicblocks with instructions and sets in 'module_func'

    ir_module_t *ir_module = &object->ir_module;

    // NOTE: These may be invalidated during statement generation
    ast_func_t *ast_func = &object->ast.funcs[ast_func_id];
    ir_func_t *module_func = &object->ir_module.funcs[ir_func_id];

    if(ast_func->statements_length == 0 && !(ast_func->traits & AST_FUNC_GENERATED) && compiler->traits & COMPILER_FUSSY){
        if(compiler_warnf(compiler, ast_func->source, "Function '%s' is empty", ast_func->name))
            return FAILURE;
    }

    // Used for constructing array of basicblocks
    ir_builder_t builder;
    ir_builder_init(&builder, compiler, object, ast_func_id, ir_func_id, job_list, false);

    while(module_func->arity != ast_func->arity){
        if(ir_gen_resolve_type(compiler, object, &ast_func->arg_types[module_func->arity], &module_func->argument_types[module_func->arity])){
            module_func->basicblocks = builder.basicblocks;
            module_func->basicblocks_length = builder.basicblocks_length;

            free(builder.block_stack_labels);
            free(builder.block_stack_break_ids);
            free(builder.block_stack_continue_ids);
            free(builder.block_stack_scopes);
            return FAILURE;
        }
        
        trait_t arg_traits = BRIDGE_VAR_UNDEF;
        if(ast_func->arg_type_traits[module_func->arity] & AST_FUNC_ARG_TYPE_TRAIT_POD) arg_traits |= BRIDGE_VAR_POD;
        add_variable(&builder, ast_func->arg_names[module_func->arity], &ast_func->arg_types[module_func->arity], module_func->argument_types[module_func->arity], arg_traits);
        module_func->arity++;
    }

    // Append variadic array argument for variadic functions
    if(ast_func->traits & AST_FUNC_VARIADIC){
        // AST variadic type is already guaranteed to exist
        add_variable(&builder, ast_func->variadic_arg_name, object->ast.common.ast_variadic_array, ir_module->common.ir_variadic_array, TRAIT_NONE);
        module_func->arity++;
    }
    
    ast_expr_t **statements = ast_func->statements;
    length_t statements_length = ast_func->statements_length;

    if(ast_func->traits & AST_FUNC_MAIN){
        // Initialize all global variables
        if(ir_gen_globals_init(&builder)){
            module_func->basicblocks = builder.basicblocks;
            module_func->basicblocks_length = builder.basicblocks_length;

            free(builder.block_stack_labels);
            free(builder.block_stack_break_ids);
            free(builder.block_stack_continue_ids);
            free(builder.block_stack_scopes);
            return FAILURE;
        }
    }

    bool terminated;
    if(ir_gen_stmts(&builder, statements, statements_length, &terminated)){
        // Make sure to update 'module_func' because ir_module.funcs may have been moved
        module_func = &object->ir_module.funcs[ir_func_id];
        module_func->basicblocks = builder.basicblocks;
        module_func->basicblocks_length = builder.basicblocks_length;

        free(builder.block_stack_labels);
        free(builder.block_stack_break_ids);
        free(builder.block_stack_continue_ids);
        free(builder.block_stack_scopes);
        return FAILURE;
    }

    // Append return instr for functions that return void
    if(!terminated){
        handle_deference_for_variables(&builder, &builder.scope->list);

        // Make sure to update references that may have been invalidated
        ast_func = &object->ast.funcs[ast_func_id];

        if(ast_func->traits & AST_FUNC_MAIN){
            handle_deference_for_globals(&builder);
            build_deinit_svars(&builder);
        }

        // Make sure to again update references that may have been invalidated
        ast_func = &object->ast.funcs[ast_func_id];

        if(ast_func->traits & AST_FUNC_AUTOGEN){
            bool failed = false;

            if(ast_func->traits & AST_FUNC_DEFER){
                failed = handle_children_deference(&builder);
            } else if(ast_func->traits & AST_FUNC_PASS){
                failed = handle_children_pass_root(&builder, false);
                if(!failed) terminated = true;
            }

            if(failed){
                // Failed to auto-generate __defer__() or __pass__() calls to children of parent type
                free(builder.block_stack_labels);
                free(builder.block_stack_break_ids);
                free(builder.block_stack_continue_ids);
                free(builder.block_stack_scopes);
                return FAILURE;
            }
        }

        // TODO: CLEANUP: Clean this up
        // We have to recheck whether the function was terminated because of 'handle_children_pass_root(&builder)'
        if(!terminated){
            // Ensure latest version of module_func reference
            ast_func = &object->ast.funcs[ast_func_id];
            module_func = &object->ir_module.funcs[ir_func_id];

            if(module_func->return_type->kind == TYPE_KIND_VOID){
                build_return(&builder, NULL);
            } else if(ast_func->traits & AST_FUNC_MAIN && module_func->return_type->kind == TYPE_KIND_S32
                        && ast_type_is_void(&ast_func->return_type)){
                // Return an int under the hood for 'func main void'
                build_return(&builder, build_literal_int(builder.pool, 0));
            } else {
                source_t where = ast_func->return_type.source;
                char *return_typename = ast_type_str(&ast_func->return_type);
                compiler_panicf(compiler, where, "Must return a value of type '%s' before exiting function '%s'", return_typename, ast_func->name);
                free(return_typename);

                module_func->basicblocks = builder.basicblocks;
                module_func->basicblocks_length = builder.basicblocks_length;

                free(builder.block_stack_labels);
                free(builder.block_stack_break_ids);
                free(builder.block_stack_continue_ids);
                free(builder.block_stack_scopes);
                return FAILURE;
            }
        }
    }

    // Make sure to update references that may have been invalidated
    module_func = &object->ir_module.funcs[ir_func_id];

    module_func->scope->following_var_id = builder.next_var_id;
    module_func->variable_count = builder.next_var_id;
    module_func->basicblocks = builder.basicblocks;
    module_func->basicblocks_length = builder.basicblocks_length;

    free(builder.block_stack_labels);
    free(builder.block_stack_break_ids);
    free(builder.block_stack_continue_ids);
    free(builder.block_stack_scopes);
    return SUCCESS;
}

errorcode_t ir_gen_job_list(object_t *object, ir_job_list_t *job_list){
    length_t *mappings_length = &object->ir_module.func_mappings_length;

    job_list->jobs = malloc(sizeof(ir_func_mapping_t) * (*mappings_length + 4));
    job_list->length = *mappings_length;
    job_list->capacity = *mappings_length + 4;

    memcpy(job_list->jobs, object->ir_module.func_mappings, sizeof(ir_func_mapping_t) * *mappings_length);
    return SUCCESS;
}

errorcode_t ir_gen_globals(compiler_t *compiler, object_t *object){
    ast_t *ast = &object->ast;
    ir_module_t *module = &object->ir_module;

    for(length_t g = 0; g != ast->globals_length; g++){
        ir_global_t *global = &module->globals[g];

        global->name = ast->globals[g].name;
        global->traits = TRAIT_NONE;

        if(ast->globals[g].traits & AST_GLOBAL_EXTERNAL) global->traits |= IR_GLOBAL_EXTERNAL;
        if(ast->globals[g].traits & AST_GLOBAL_THREAD_LOCAL) global->traits |= IR_GLOBAL_THREAD_LOCAL;
        
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

        if(ir_gen_expr(builder, ast_global->initial, &value, false, &value_ast_type)) return FAILURE;

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
        internalerrorprintf("Failed to get IR type for special global variable\n");
        return FAILURE;
    }

    // NOTE: DANGEROUS: 'global_variable_id' is assumed to be the same between AST and IR global variable lists
    ir_global_t *ir_global = &ir_module->globals[global_variable_id];

    if(ast_global->traits & AST_GLOBAL___TYPES__){
        return ir_gen__types__(compiler, object, ir_global);
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
            internalerrorprintf("Failed to find types used by the runtime type table that should've been injected\n");
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
    internalerrorprintf("Encountered unknown special global variable '%s'\n", ast_global->name);
    return FAILURE;
}

errorcode_t ir_gen_fill_in_rtti(object_t *object){
    ir_module_t *ir_module = &object->ir_module;

    type_table_t *type_table = object->ast.type_table;
    if(type_table == NULL) return FAILURE;

    rtti_relocation_t *relocations = ir_module->rtti_relocations;
    length_t relocations_length = ir_module->rtti_relocations_length;

    for(length_t i = 0; i != relocations_length; i++){
        if(rtti_resolve(type_table, &relocations[i])) return FAILURE;
    }

    return SUCCESS;
}

weak_cstr_t ir_gen_ast_definition_string(ir_pool_t *pool, ast_func_t *ast_func){
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

errorcode_t ir_gen_do_builtin_warn_bad_printf_format(ir_builder_t *builder, funcpair_t pair, ast_type_t *ast_types, ir_value_t **ir_values, source_t source, length_t variadic_length){
    // Find index of 'format' argument
    maybe_index_t format_index = -1;

    for(length_t name_index = 0; name_index != pair.ast_func->arity; name_index++){
        if(strcmp(pair.ast_func->arg_names[name_index], "format") == 0){
            format_index = name_index;
            break;
        }
    }

    if(format_index < 0){
        compiler_panicf(builder->compiler, pair.ast_func->source, "Function marked as __builtin_warn_bad_printf must have an argument named 'format'!\n");
        return FAILURE;
    }

    if(!ast_type_is_base_of(&pair.ast_func->arg_types[format_index], "String")){
        compiler_panicf(builder->compiler, pair.ast_func->source, "Function marked as __builtin_warn_bad_printf must have 'format' be a String!\n");
        return FAILURE;
    }

    // Undo __pass__ call
    if(ir_values[format_index]->value_type != VALUE_TYPE_RESULT) return SUCCESS;
    ir_value_result_t *pass_result = (ir_value_result_t*) ir_values[format_index]->extra;
    
    // Undo result value to get call instruction
    ir_instr_call_t *call_instr = (ir_instr_call_t*) builder->basicblocks[pass_result->block_id].instructions[pass_result->instruction_id];
    ir_value_t *string_literal = call_instr->values[0];

    // Don't check if not string literal
    if(string_literal->value_type != VALUE_TYPE_STRUCT_LITERAL) return SUCCESS;

    // DANGEROUS:
    // Assuming string literal is created correctly since the AST type is 'String' and IR value type kind is VALUE_TYPE_STRUCT_LITERAL
    ir_value_struct_literal_t *extra = (ir_value_struct_literal_t*) string_literal->extra;
    
    ir_value_t *cstr_of_len_literal = extra->values[0];
    if(cstr_of_len_literal->value_type != VALUE_TYPE_CSTR_OF_LEN) return SUCCESS;
    
    ir_value_cstr_of_len_t *string = (ir_value_cstr_of_len_t*) cstr_of_len_literal->extra;
    length_t substitutions_gotten = 0;

    char *p = string->array;
    char *end = &string->array[string->length];

    while(p != end){
        if(*p++ != '%') continue;

        if(p == end || *p == '%') break;

        // size_modifier == 1 for 'hh'
        // size_modifier == 2 for 'h'
        // size_modifier == 4 for none
        // size_modifier == 8 for 'l' or 'll'
        int size_modifier = 4;

        while(p != end){
            if(*p == 'l'){
                size_modifier *= 2;
                if(size_modifier > 8) size_modifier = 8;
                p++;
                continue;
            }

            if(*p == 'h'){
                size_modifier /= 2;
                if(size_modifier == 0) size_modifier = 1;
                p++;
                continue;
            }
            break;
        }
        if(p == end) break;

        if(substitutions_gotten >= variadic_length){
            strong_cstr_t escaped = string_to_escaped_string(string->array, string->length, '"');
            compiler_panicf(builder->compiler, source, "Not enough arguments specified for format %s", escaped);
            free(escaped);
            return FAILURE;
        }

        ast_type_t *given_type = &ast_types[pair.ast_func->arity + substitutions_gotten++];
        weak_cstr_t target = NULL;

        switch(*p++){
        case 'S':
            if(!ast_type_is_base_of(given_type, "String")){
                bad_printf_format(builder->compiler, source, given_type, "String", substitutions_gotten, size_modifier, *(p - 1), false);
                return FAILURE;
            }
            break;
        case 'b': case 'B': case 'y': case 'Y':
            if(!ast_type_is_base_of(given_type, "bool")){
                bad_printf_format(builder->compiler, source, given_type, "bool", substitutions_gotten, size_modifier, *(p - 1), false);
                return FAILURE;
            }
            break;
        case 's':
            if(!ast_type_is_base_ptr_of(given_type, "ubyte")){
                bad_printf_format(builder->compiler, source, given_type, "*ubyte", substitutions_gotten, size_modifier, *(p - 1), false);
                return FAILURE;
            }
            break;
        case 'c':
            if(!ast_type_is_base_of(given_type, "ubyte")){
                bad_printf_format(builder->compiler, source, given_type, "ubyte", substitutions_gotten, size_modifier, *(p - 1), false);
                return FAILURE;
            }
            break;
        case 'z':
            if(*p++ != 'u'){
                compiler_panicf(builder->compiler, source, "Expected 'u' after '%%z' in format string");
                return FAILURE;
            }

            if(!ast_type_is_base_of(given_type, "usize")){
                bad_printf_format(builder->compiler, source, given_type, "usize", substitutions_gotten, size_modifier, *(p - 2), false);
                return FAILURE;
            }
            break;
        case 'd': case 'i':
            switch(size_modifier){
            case 1:  target = "byte";  break;
            case 2:  target = "short"; break;
            case 8:  target = "long";  break;
            default: target = "int";
            }

            if(ast_type_is_base_of(given_type, target)){
                // Always allowed
            } else if(
                ast_type_is_base_of(given_type, "bool") ||
                ast_type_is_base_of(given_type, "byte") || 
                ast_type_is_base_of(given_type, "ubyte") || 
                ast_type_is_base_of(given_type, "short") || 
                ast_type_is_base_of(given_type, "ushort") ||
                ast_type_is_base_of(given_type, "int") ||
                ast_type_is_base_of(given_type, "uint") || 
                ast_type_is_base_of(given_type, "long") || 
                ast_type_is_base_of(given_type, "ulong") || 
                ast_type_is_base_of(given_type, "usize") || 
                ast_type_is_base_of(given_type, "successful")
            ){
                // Allowed, but discouraged (Error if RTTI is disabled)
                if(builder->compiler->traits & COMPILER_NO_TYPEINFO){
                    bad_printf_format(builder->compiler, source, given_type, target, substitutions_gotten, size_modifier, *(p - 1), true);
                    return FAILURE;
                }
            } else {
                // Never allowed
                bad_printf_format(builder->compiler, source, given_type, target, substitutions_gotten, size_modifier, *(p - 1), false);
                return FAILURE;
            }
            break;
        case 'u':
            switch(size_modifier){
            case 1:  target = "ubyte";  break;
            case 2:  target = "ushort"; break;
            case 8:  target = "ulong";  break;
            default: target = "uint";
            }

            if(ast_type_is_base_of(given_type, target) || (size_modifier == 8 && ast_type_is_base_of(given_type, "usize"))){
                // Always allowed
            } else if(
                ast_type_is_base_of(given_type, "bool") ||
                ast_type_is_base_of(given_type, "byte") || 
                ast_type_is_base_of(given_type, "ubyte") || 
                ast_type_is_base_of(given_type, "short") || 
                ast_type_is_base_of(given_type, "ushort") || 
                ast_type_is_base_of(given_type, "int") || 
                ast_type_is_base_of(given_type, "uint") || 
                ast_type_is_base_of(given_type, "long") || 
                ast_type_is_base_of(given_type, "ulong") || 
                ast_type_is_base_of(given_type, "usize") || 
                ast_type_is_base_of(given_type, "successful")
            ){
                // Allowed, but discouraged (Error if RTTI is disabled)
                if(builder->compiler->traits & COMPILER_NO_TYPEINFO){
                    bad_printf_format(builder->compiler, source, given_type, target, substitutions_gotten, size_modifier, *(p - 1), true);
                    return FAILURE;
                }
            } else {
                // Never allowed
                bad_printf_format(builder->compiler, source, given_type, target, substitutions_gotten, size_modifier, *(p - 1), false);
                return FAILURE;
            }
            break;
        case 'f':
            switch(size_modifier){
            case 2:  target = "float";  break;
            default: target = "double";
            }
            if(ast_type_is_base_of(given_type, target)){
                // Always allowed
            } else if(
                ast_type_is_base_of(given_type, "double") ||
                ast_type_is_base_of(given_type, "float") ||
                ast_type_is_base_of(given_type, "bool") ||
                ast_type_is_base_of(given_type, "byte") || 
                ast_type_is_base_of(given_type, "ubyte") || 
                ast_type_is_base_of(given_type, "short") || 
                ast_type_is_base_of(given_type, "ushort") || 
                ast_type_is_base_of(given_type, "int") || 
                ast_type_is_base_of(given_type, "uint") || 
                ast_type_is_base_of(given_type, "long") || 
                ast_type_is_base_of(given_type, "ulong") || 
                ast_type_is_base_of(given_type, "usize") || 
                ast_type_is_base_of(given_type, "successful")
            ){
                // Allowed, but discouraged (Error if RTTI is disabled)
                if(builder->compiler->traits & COMPILER_NO_TYPEINFO){
                    bad_printf_format(builder->compiler, source, given_type, target, substitutions_gotten, size_modifier, *(p - 1), true);
                    return FAILURE;
                }
            } else {
                // Never allowed
                bad_printf_format(builder->compiler, source, given_type, target, substitutions_gotten, size_modifier, *(p - 1), false);
                return FAILURE;
            }
            break;
        case 'p':
            if(!ast_type_is_pointer(given_type) && !ast_type_is_base_of(given_type, "ptr")){
                // Never allowed
                bad_printf_format(builder->compiler, source, given_type, "ptr' or '*T", substitutions_gotten, size_modifier, *(p - 1), false);
                return FAILURE;
            }
            break;
        default:
            if(compiler_warnf(builder->compiler, source, "Unrecognized format specifier '%%%c'", *(p - 1)))
                return FAILURE;
        }
    }

    if(substitutions_gotten < variadic_length){
        strong_cstr_t escaped = string_to_escaped_string(string->array, string->length, '"');
        compiler_panicf(builder->compiler, source, "Too many arguments specified for format %s", escaped);
        free(escaped);
        return FAILURE;
    }

    return SUCCESS;
}

void bad_printf_format(compiler_t *compiler, source_t source, ast_type_t *given_type, weak_cstr_t expected, int variadic_argument_number, int size_modifier, char specifier, bool is_semimatch){
    weak_cstr_t modifiers = "";
    weak_cstr_t additional_part = "";
    
    switch(size_modifier){
    case 1: modifiers = "hh"; break;
    case 2: modifiers = "h";  break;
    case 8: modifiers = "l";  break;
    }

    // Hack to get '%z' to always print as '%zu'
    if(specifier == 'z') additional_part = "u";

    strong_cstr_t incorrect_type = ast_type_str(given_type);
    compiler_panicf(compiler, source, "Got value of incorrect type for format specifier '%%%s%c%s'", modifiers, specifier, additional_part);
    printf("\n");

    printf("    Expected value of type '%s', got value of type '%s'\n", expected, incorrect_type);
    printf("    For %d%s variadic argument\n", variadic_argument_number, get_numeric_ending(variadic_argument_number));
    
    if(is_semimatch){
        printf("    Support for this type mismatch requires runtime type information\n");
    }

    free(incorrect_type);
}

weak_cstr_t get_numeric_ending(length_t integer){
    if(integer > 9 && integer < 11) return "th";

    switch(integer % 10){
    case 1: return "st";
    case 2: return "nd";
    case 3: return "rd";
    }

    return "th";
}
