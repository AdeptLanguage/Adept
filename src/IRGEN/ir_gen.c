
#include "UTIL/ground.h"
#include "UTIL/search.h"
#include "UTIL/filename.h"
#include "IRGEN/ir_gen.h"
#include "IRGEN/ir_gen_expr.h"
#include "IRGEN/ir_gen_stmt.h"
#include "IRGEN/ir_gen_type.h"

int ir_gen(compiler_t *compiler, object_t *object){
    ir_module_t *module = &object->ir_module;
    ast_t *ast = &object->ast;

    ir_pool_init(&module->pool);
    module->funcs = malloc(sizeof(ir_func_t) * ast->funcs_length);
    module->funcs_length = 0;
    module->func_mappings = NULL;
    module->methods = NULL;
    module->methods_length = 0;
    module->methods_capacity = 0;
    module->type_map.mappings = NULL;
    module->globals = malloc(sizeof(ir_global_t) * ast->globals_length);
    module->globals_length = 0;

    // Initialize common data
    module->common.ir_funcptr_type = NULL;
    module->common.ir_usize_type = NULL;
    module->common.ir_usize_ptr_type = NULL;
    module->common.ir_bool_type = NULL;

    object->compilation_stage = COMPILATION_STAGE_IR_MODULE;
    if(ir_gen_type_mappings(compiler, object)) return 1; // Generate type mappings

    if(ir_gen_globals(compiler, object)) return 1; // ir_gen global variables
    if(ir_gen_functions(compiler, object)) return 1; // ir_gen function skeltons
    if(ir_gen_functions_body(compiler, object)) return 1; // ir_gen function bodies
    return 0;
}

int ir_gen_functions(compiler_t *compiler, object_t *object){
    // NOTE: Only ir_gens function skeletons

    ast_t *ast = &object->ast;
    ir_module_t *module = &object->ir_module;
    ast_func_t *ast_funcs;
    ir_func_t *module_funcs;
    length_t ast_funcs_length;
    length_t *module_funcs_length;

    ast_funcs = ast->funcs;
    module_funcs = module->funcs;
    ast_funcs_length = ast->funcs_length;
    module_funcs_length = &module->funcs_length;
    module->func_mappings = malloc(sizeof(ir_func_mapping_t) * ast->funcs_length);

    for(length_t f = 0; f != ast_funcs_length; f++){
        ast_func_t *ast_func = &ast_funcs[f];
        ir_func_t *module_func = &module_funcs[f];

        module_func->name = ast_func->name;
        module_func->traits = TRAIT_NONE;
        module_func->return_type = NULL;
        module_func->argument_types = malloc(sizeof(ir_type_t*) * ast_func->arity);
        module_func->arity = 0;
        module_func->basicblocks = NULL; // Will be set after 'basicblocks' contains all of the basicblocks
        module_func->basicblocks_length = 0; // Will be set after 'basicblocks' contains all of the basicblocks
        module_func->var_scope = NULL;
        module_func->variable_count = 0;
        module->func_mappings[f].name = ast_func->name;
        module->func_mappings[f].ast_func = ast_func;
        module->func_mappings[f].module_func = module_func;
        module->func_mappings[f].func_id = f;
        module->func_mappings[f].is_beginning_of_group = -1;
        (*module_funcs_length)++;

        if(ast_func->traits & AST_FUNC_FOREIGN) module_func->traits |= IR_FUNC_FOREIGN;
        if(ast_func->traits & AST_FUNC_VARARG)  module_func->traits |= IR_FUNC_VARARG;
        if(ast_func->traits & AST_FUNC_STDCALL) module_func->traits |= IR_FUNC_STDCALL;

        if(!(ast_func->traits & AST_FUNC_FOREIGN)){
            if(ast_func->arity > 0 && strcmp(ast_func->arg_names[0], "this") == 0){
                // This is a struct method, attach a reference to the struct
                ast_type_t *this_type = &ast_func->arg_types[0];

                // Do basic checking to make sure the type is in the format: *Structure
                if(this_type->elements_length != 2 || this_type->elements[0]->id != AST_ELEM_POINTER
                        || this_type->elements[1]->id != AST_ELEM_BASE){
                    compiler_panic(compiler, this_type->source, "Type of 'this' must be a pointer to a struct");
                    return 1;
                }

                // Check that the base isn't a primitive
                const length_t primitives_length = 12;
                const char * const primitives[] = {
                    "bool", "byte", "double", "float", "int", "long", "short", "ubyte", "uint", "ulong", "ushort", "usize"
                };

                int array_index = binary_string_search(primitives, primitives_length, ((ast_elem_base_t*) this_type->elements[1])->base);
                if(array_index != -1){
                    compiler_panicf(compiler, this_type->source, "Type of 'this' must be a pointer to a struct (%s is a primitive)", primitives[array_index]);
                    return 1;
                }

                // Find the target structure
                ast_struct_t *target = ast_struct_find(ast, ((ast_elem_base_t*) this_type->elements[1])->base);
                if(target == NULL){
                    compiler_panicf(compiler, this_type->source, "Undeclared struct '%s'", ((ast_elem_base_t*) this_type->elements[1])->base);
                    return 1;
                }

                // Append the method to the struct's method list
                if(module->methods == NULL){
                    module->methods = malloc(sizeof(ir_method_t) * 4);
                    module->methods_length = 0;
                    module->methods_capacity = 4;
                } else if(module->methods_length == module->methods_capacity){
                    module->methods_capacity *= 2;
                    ir_method_t *new_methods = malloc(sizeof(ir_method_t) * module->methods_capacity);
                    memcpy(new_methods, module->methods, sizeof(ir_method_t) * module->methods_length);
                    free(module->methods);
                    module->methods = new_methods;
                }

                ir_method_t *method = &module->methods[module->methods_length++];
                method->struct_name = ((ast_elem_base_t*) this_type->elements[1])->base;
                method->name = module_func->name;
                method->ast_func = ast_func;
                method->module_func = module_func;
                method->func_id = f;
                method->is_beginning_of_group = -1;
            }
        } else {
            while(module_func->arity != ast_func->arity){
                if(ir_gen_resolve_type(compiler, object, &ast_func->arg_types[module_func->arity], &module_func->argument_types[module_func->arity])) return 1;
                module_func->arity++;
            }
        }

        if(ir_gen_resolve_type(compiler, object, &ast_func->return_type, &module_func->return_type)) return 1;
    }

    qsort(module->func_mappings, ast->funcs_length, sizeof(ir_func_mapping_t), ir_func_mapping_cmp);
    qsort(module->methods, module->methods_length, sizeof(ir_method_t), ir_method_cmp);
    return 0;
}

int ir_gen_functions_body(compiler_t *compiler, object_t *object){
    // NOTE: Only ir_gens function body; assumes skeleton already exists

    ast_t *ast = &object->ast;
    ir_module_t *module = &object->ir_module;

    ast_func_t *ast_funcs = ast->funcs;
    ir_func_t *module_funcs = module->funcs;
    length_t ast_funcs_length = ast->funcs_length;

    for(length_t f = 0; f != ast_funcs_length; f++){
        if(ast_funcs[f].traits & AST_FUNC_FOREIGN) continue;
        if(ir_gen_func_statements(compiler, object, &ast_funcs[f], &module_funcs[f])) return 1;
    }

    return 0;
}

int ir_gen_globals(compiler_t *compiler, object_t *object){
    ast_t *ast = &object->ast;
    ir_module_t *module = &object->ir_module;

    for(length_t g = 0; g != ast->globals_length; g++){
        module->globals[g].name = ast->globals[g].name;
        if(ir_gen_resolve_type(compiler, object, &ast->globals[g].type, &module->globals[g].type)) return 1;
        module->globals_length++;
    }

    return 0;
}

int ir_gen_globals_init(ir_builder_t *builder){
    // Generates instructions for initializing global variables
    ast_global_t *globals = builder->object->ast.globals;
    length_t globals_length = builder->object->ast.globals_length;

    for(length_t g = 0; g != globals_length; g++){
        ast_global_t *ast_global = &globals[g];
        ir_value_t *destination;
        ir_value_t *value;
        ast_type_t value_ast_type;
        ir_instr_t* instruction;

        if(ast_global->initial == NULL) continue;
        if(ir_gen_expression(builder, ast_global->initial, &value, false, &value_ast_type)) return 1;

        if(!ast_types_conform(builder, &value, &value_ast_type, &ast_global->type, CONFORM_MODE_PRIMITIVES)){
            char *a_type_str = ast_type_str(&value_ast_type);
            char *b_type_str = ast_type_str(&ast_global->type);
            compiler_panicf(builder->compiler, ast_global->initial->source, "Incompatible types '%s' and '%s'", a_type_str, b_type_str);
            free(a_type_str);
            free(b_type_str);
            ast_type_free(&value_ast_type);
            return 1;
        }

        ast_type_free(&value_ast_type);

        ir_global_t *ir_global = &builder->object->ir_module.globals[g];
        ir_type_t *global_pointer_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
        global_pointer_type->kind = TYPE_KIND_POINTER;
        global_pointer_type->extra = ir_global->type;

        ir_basicblock_new_instructions(builder->current_block, 1);
        instruction = (ir_instr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_varptr_t));
        ((ir_instr_varptr_t*) instruction)->id = INSTRUCTION_GLOBALVARPTR;
        ((ir_instr_varptr_t*) instruction)->result_type = global_pointer_type;
        ((ir_instr_varptr_t*) instruction)->index = g;
        builder->current_block->instructions[builder->current_block->instructions_length++] = instruction;
        destination = build_value_from_prev_instruction(builder);

        ir_basicblock_new_instructions(builder->current_block, 1);
        instruction = (ir_instr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_store_t));
        ((ir_instr_store_t*) instruction)->id = INSTRUCTION_STORE;
        ((ir_instr_store_t*) instruction)->result_type = NULL; // For safety
        ((ir_instr_store_t*) instruction)->value = value;
        ((ir_instr_store_t*) instruction)->destination = destination;
        builder->current_block->instructions[builder->current_block->instructions_length++] = instruction;
    }
    return 0;
}

int ir_func_mapping_cmp(const void *a, const void *b){
    int diff = strcmp(((ir_func_mapping_t*) a)->module_func->name, ((ir_func_mapping_t*) b)->module_func->name);
    if(diff != 0) return diff;
    return (int) ((ir_func_mapping_t*) a)->func_id - (int) ((ir_func_mapping_t*) b)->func_id;
}

int ir_method_cmp(const void *a, const void *b){
    int diff = strcmp(((ir_method_t*) a)->struct_name, ((ir_method_t*) b)->struct_name);
    if(diff != 0) return diff;
    diff = strcmp(((ir_method_t*) a)->name, ((ir_method_t*) b)->name);
    if(diff != 0) return diff;
    return (int) ((ir_method_t*) a)->func_id - (int) ((ir_method_t*) b)->func_id;
}
