
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AST/POLY/ast_resolve.h"
#include "AST/TYPE/ast_type_identical.h"
#include "AST/TYPE/ast_type_make.h"
#include "AST/ast.h"
#include "AST/ast_expr.h"
#include "AST/ast_layout.h"
#include "AST/ast_poly_catalog.h"
#include "AST/ast_type.h"
#include "BRIDGE/bridge.h"
#include "DRVR/compiler.h"
#include "DRVR/object.h"
#include "IR/ir.h"
#include "IR/ir_func_endpoint.h"
#include "IR/ir_pool.h"
#include "IR/ir_type.h"
#include "IR/ir_value.h"
#include "IRGEN/ir_builder.h"
#include "IRGEN/ir_cache.h"
#include "IRGEN/ir_gen.h"
#include "IRGEN/ir_gen_expr.h"
#include "IRGEN/ir_gen_find.h"
#include "IRGEN/ir_gen_find_sf.h"
#include "IRGEN/ir_gen_type.h"
#include "LEX/lex.h"
#include "UTIL/builtin_type.h"
#include "UTIL/color.h"
#include "UTIL/datatypes.h"
#include "UTIL/func_pair.h"
#include "UTIL/ground.h"
#include "UTIL/string.h"
#include "UTIL/trait.h"

void ir_builder_init(ir_builder_t *builder, compiler_t *compiler, object_t *object, func_id_t ast_func_id, func_id_t ir_func_id, bool static_builder){
    builder->basicblocks = (ir_basicblocks_t){
        .blocks = malloc(sizeof(ir_basicblock_t) * 4),
        .length = 1,
        .capacity = 4,
    };

    builder->pool = &object->ir_module.pool;
    builder->type_map = &object->ir_module.type_map;
    builder->compiler = compiler;
    builder->object = object;
    builder->ast_func_id = ast_func_id;
    builder->ir_func_id = ir_func_id;
    builder->next_var_id = 0;
    builder->has_string_struct = TROOLEAN_UNKNOWN;

    ir_basicblock_t *entry_block = &builder->basicblocks.blocks[0];

    *entry_block = (ir_basicblock_t){
        .instructions = ir_instrs_create(16),
        .traits = TRAIT_NONE,
    };

    // Set current block
    builder->current_block = entry_block;
    builder->current_block_id = 0;

    // Zero indicates no block to continue/break to since block 0 would make no sense
    builder->break_block_id = 0;
    builder->continue_block_id = 0;
    builder->fallthrough_block_id = 0;

    // Block stack, used for breaking and continuing by label
    // NOTE: Unlabeled blocks won't go in this array
    builder->block_stack = (block_stack_t){0};

    if(!static_builder){
        ir_func_t *module_func = &object->ir_module.funcs.funcs[ir_func_id];
        module_func->scope = malloc(sizeof(bridge_scope_t));
        bridge_scope_init(module_func->scope, NULL);
        module_func->scope->first_var_id = 0;
        builder->scope = module_func->scope;
    } else {
        builder->scope = NULL;
    }

    builder->job_list = &object->ir_module.job_list;

    source_t object_source = { .index = 0, .object_index = builder->object->index, .stride = 0 };

    builder->static_bool_base = (ast_elem_base_t){
        .id = AST_ELEM_BASE,
        .source = object_source,
        .base = "bool",
    };

    builder->static_bool_elems = (ast_elem_t*) &builder->static_bool_base;

    builder->static_bool = (ast_type_t){
        .elements = &builder->static_bool_elems,
        .elements_length = 1,
        .source = object_source,
    };

    builder->s8_type = ir_type_make(builder->pool, TYPE_KIND_S8, NULL);

    builder->ptr_type = ir_type_make_pointer_to(builder->pool, builder->s8_type);
    builder->type_table = object->ast.type_table;
    builder->has_noop_defer_function = false;
    builder->noop_defer_function = 0;
}

void ir_builder_free(ir_builder_t *builder){
    // TODO: Work in progress to make unified ir_builder_free function
    
    // The primary builder has
    free(builder->block_stack.blocks);

    // The two init/deinit builders have
    ir_basicblocks_free(&builder->object->ir_module.init_builder->basicblocks);
}

length_t build_basicblock(ir_builder_t *builder){
    // NOTE: Returns new id
    // NOTE: All basicblock pointers should be recalculated after calling this function
    //           (Except for 'builder->current_block' which is automatically recalculated if necessary)

    ir_basicblocks_t *container = &builder->basicblocks;

    ir_basicblocks_append(container, ((ir_basicblock_t){
        .instructions = ir_instrs_create(16),
        .traits = TRAIT_NONE
    }));

    // Update current block pointer
    builder->current_block = &container->blocks[builder->current_block_id];

    // Return newest block id
    return container->length - 1;
}

void build_using_basicblock(ir_builder_t *builder, length_t basicblock_id){
    // NOTE: Sets basicblock that instructions will be inserted into
    builder->current_block = &builder->basicblocks.blocks[basicblock_id];
    builder->current_block_id = basicblock_id;
}

ir_instr_t* build_instruction(ir_builder_t *builder, length_t size){
    // NOTE: Builds an empty instruction of the size 'size'
    ir_instrs_t *instrs = &builder->current_block->instructions;
    ir_instrs_append(instrs, (ir_instr_t*) ir_pool_alloc(builder->pool, size));
    return ir_instrs_last_unchecked(instrs);
}

ir_value_t *build_value_from_prev_instruction(ir_builder_t *builder){
    // Builds an ir_value_t for the result of the last instruction in the current block

    ir_instrs_t *instrs = &builder->current_block->instructions;

    if(instrs->length == 0){
        die("build_value_from_prev_instruction() cannot be called when no instructions present\n");
    }

    length_t instruction_id = instrs->length - 1;
    ir_type_t *result_type = instrs->instructions[instruction_id]->result_type;

    if(result_type == NULL){
        internalerrorprintf("build_value_from_prev_instruction() cannot create value for instruction that doesn't have a result\n");
        redprintf("Previous instruction id: 0x%08X\n", (int) instruction_id);
        die("Terminating...\n");
    }

    return ir_pool_alloc_init(builder->pool, ir_value_t, {
        .value_type = VALUE_TYPE_RESULT,
        .type = result_type,
        .extra = ir_pool_alloc_init(builder->pool, ir_value_result_t, {
            .block_id = builder->current_block_id,
            .instruction_id = instruction_id,
        })
    });
}

ir_value_t *build_varptr(ir_builder_t *builder, ir_type_t *ptr_type, bridge_var_t *var){
    if(var->traits & BRIDGE_VAR_STATIC){
        return build_svarptr(builder, ptr_type, var->static_id);
    } else {
        return build_lvarptr(builder, ptr_type, var->id);
    }
}

ir_value_t *build_lvarptr(ir_builder_t *builder, ir_type_t *ptr_type, length_t variable_id){
    ir_instr_varptr_t *instruction = (ir_instr_varptr_t*) build_instruction(builder, sizeof(ir_instr_varptr_t));

    *instruction = (ir_instr_varptr_t){
        .id = INSTRUCTION_VARPTR,
        .result_type = ptr_type,
        .index = variable_id,
    };

    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_gvarptr(ir_builder_t *builder, ir_type_t *ptr_type, length_t variable_id){
    ir_instr_varptr_t *instruction = (ir_instr_varptr_t*) build_instruction(builder, sizeof(ir_instr_varptr_t));

    *instruction = (ir_instr_varptr_t){
        .id = INSTRUCTION_GLOBALVARPTR,
        .result_type = ptr_type,
        .index = variable_id,
    };

    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_svarptr(ir_builder_t *builder, ir_type_t *ptr_type, length_t variable_id){
    ir_instr_varptr_t *instruction = (ir_instr_varptr_t*) build_instruction(builder, sizeof(ir_instr_varptr_t));

    *instruction = (ir_instr_varptr_t){
        .id = INSTRUCTION_STATICVARPTR,
        .result_type = ptr_type,
        .index = variable_id,
    };

    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_malloc(ir_builder_t *builder, ir_type_t *type, ir_value_t *amount, bool is_undef){
    ir_instr_malloc_t *instruction = (ir_instr_malloc_t*) build_instruction(builder, sizeof(ir_instr_malloc_t));

    *instruction = (ir_instr_malloc_t){
        .id = INSTRUCTION_MALLOC,
        .result_type = ir_type_make_pointer_to(builder->pool, type),
        .type = type,
        .amount = amount,
        .is_undef = is_undef,
    };

    return build_value_from_prev_instruction(builder);
}

void build_zeroinit(ir_builder_t *builder, ir_value_t *destination){
    ir_instr_zeroinit_t *instruction = (ir_instr_zeroinit_t*) build_instruction(builder, sizeof(ir_instr_zeroinit_t));

    *instruction = (ir_instr_zeroinit_t){
        .id = INSTRUCTION_ZEROINIT,
        .destination = destination,
    };
}

void build_memcpy(ir_builder_t *builder, ir_value_t *destination, ir_value_t *value, ir_value_t *num_bytes, bool is_volatile){
    ir_instr_memcpy_t *instruction = (ir_instr_memcpy_t*) build_instruction(builder, sizeof(ir_instr_memcpy_t));

    *instruction = (ir_instr_memcpy_t){
        .id = INSTRUCTION_MEMCPY,
        .result_type = NULL,
        .destination = destination,
        .value = value,
        .bytes = num_bytes,
        .is_volatile = is_volatile,
    };
}

ir_value_t *build_load(ir_builder_t *builder, ir_value_t *value, source_t code_source){
    // The value provided must be dereferencable
    ir_type_t *dereferenced_type = ir_type_dereference(value->type);
    if(dereferenced_type == NULL) return NULL;

    ir_instr_load_t *instruction = (ir_instr_load_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_load_t));

    *instruction = (ir_instr_load_t){
        .id = INSTRUCTION_LOAD,
        .result_type = dereferenced_type,
        .value = value,
    };

    if(builder->compiler->checks & COMPILER_NULL_CHECKS) {
        // If we're doing null checks, then give line/column to IR load instruction.
        lex_get_location(builder->compiler->objects[code_source.object_index]->buffer, code_source.index, &instruction->maybe_line_number, &instruction->maybe_column_number);
    }

    // Otherwise, we just ignore line/column fields
    ir_instrs_append(&builder->current_block->instructions, (ir_instr_t*) instruction);
    return build_value_from_prev_instruction(builder);
}

ir_instr_store_t *build_store(ir_builder_t *builder, ir_value_t *value, ir_value_t *destination, source_t code_source){
    ir_instr_store_t *instruction = (ir_instr_store_t*) build_instruction(builder, sizeof(ir_instr_store_t));

    *instruction = (ir_instr_store_t){
        .id = INSTRUCTION_STORE,
        .result_type = NULL,
        .value = value,
        .destination = destination,
    };

    // If we're doing null checks, then give line/column to IR store instruction.
    if(builder->compiler->checks & COMPILER_NULL_CHECKS) {
        lex_get_location(builder->compiler->objects[code_source.object_index]->buffer, code_source.index, &instruction->maybe_line_number, &instruction->maybe_column_number);
    }

    return instruction;
}

ir_value_t *build_call(ir_builder_t *builder, func_id_t ir_func_id, ir_type_t *result_type, ir_value_t **arguments, length_t arguments_length){
    ir_instr_call_t *instruction = (ir_instr_call_t*) build_instruction(builder, sizeof(ir_instr_call_t));

    *instruction = (ir_instr_call_t){
        .id = INSTRUCTION_CALL,
        .result_type = result_type,
        .values = arguments,
        .values_length = arguments_length,
        .ir_func_id = ir_func_id,
    };

    return build_value_from_prev_instruction(builder);
}

void build_call_ignore_result(ir_builder_t *builder, func_id_t ir_func_id, ir_type_t *result_type, ir_value_t **arguments, length_t arguments_length){
    ir_instr_call_t *instruction = (ir_instr_call_t*) build_instruction(builder, sizeof(ir_instr_call_t));

    *instruction = (ir_instr_call_t){
        .id = INSTRUCTION_CALL,
        .result_type = result_type,
        .values = arguments,
        .values_length = arguments_length,
        .ir_func_id = ir_func_id,
    };
}

ir_value_t *build_call_address(ir_builder_t *builder, ir_type_t *return_type, ir_value_t *address, ir_value_t **arg_values, length_t arity){
    ir_instr_call_address_t *instruction = (ir_instr_call_address_t*) build_instruction(builder, sizeof(ir_instr_call_address_t));

    *instruction = (ir_instr_call_address_t){
        .id = INSTRUCTION_CALL_ADDRESS,
        .result_type = return_type,
        .address = address,
        .values = arg_values,
        .values_length = arity,
    };

    return build_value_from_prev_instruction(builder);
}

void build_break(ir_builder_t *builder, length_t basicblock_id){
    ir_instr_break_t *instruction = (ir_instr_break_t*) build_instruction(builder, sizeof(ir_instr_break_t));

    *instruction = (ir_instr_break_t){
        .id = INSTRUCTION_BREAK,
        .result_type = NULL,
        .block_id = basicblock_id,
    };
}

void build_cond_break(ir_builder_t *builder, ir_value_t *condition, length_t true_block_id, length_t false_block_id){
    ir_instr_cond_break_t *instruction = (ir_instr_cond_break_t*) build_instruction(builder, sizeof(ir_instr_cond_break_t));

    *instruction = (ir_instr_cond_break_t){
        .id = INSTRUCTION_CONDBREAK,
        .result_type = NULL,
        .value = condition,
        .true_block_id = true_block_id,
        .false_block_id = false_block_id,
    };
}

ir_value_t *build_equals(ir_builder_t *builder, ir_value_t *a, ir_value_t *b){
    ir_instr_math_t *instruction = (ir_instr_math_t*) build_instruction(builder, sizeof(ir_instr_math_t));

    *instruction = (ir_instr_math_t){
        .id = INSTRUCTION_EQUALS,
        .a = a,
        .b = b,
        .result_type = ir_builder_bool(builder),
    };

    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_array_access(ir_builder_t *builder, ir_value_t *value, ir_value_t *index, source_t code_source){
    if(value->type->kind != TYPE_KIND_POINTER){
        internalerrorprintf("build_array_access() - Cannot perform array access on non-pointer value\n");
        redprintf("                (value left unmodified)\n");
        return value;
    }

    return build_array_access_ex(builder, value, index, value->type, code_source);
}

ir_value_t *build_array_access_ex(ir_builder_t *builder, ir_value_t *value, ir_value_t *index, ir_type_t *result_type, source_t code_source){
    ir_instr_array_access_t *instruction = (ir_instr_array_access_t*) build_instruction(builder, sizeof(ir_instr_array_access_t));

    *instruction = (ir_instr_array_access_t){
        .id = INSTRUCTION_ARRAY_ACCESS,
        .result_type = result_type,
        .value = value,
        .index = index,
        .maybe_line_number = -1,
        .maybe_column_number = -1,
    };

    // If we're doing null checks, then give line/column to IR array access instruction.
    if(builder->compiler->checks & COMPILER_NULL_CHECKS) {
        const char *const buffer = builder->compiler->objects[code_source.object_index]->buffer;
        lex_get_location(buffer, code_source.index, &instruction->maybe_line_number, &instruction->maybe_column_number);
    }

    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_member(ir_builder_t *builder, ir_value_t *value, length_t member, ir_type_t *result_type, source_t code_source){
    ir_instr_member_t *instruction = (ir_instr_member_t*) build_instruction(builder, sizeof(ir_instr_member_t));

    *instruction = (ir_instr_member_t){
        .id = INSTRUCTION_MEMBER,
        .result_type = result_type,
        .value = value,
        .member = member,
        .maybe_line_number = -1,
        .maybe_column_number = -1,
    };

    // If we're doing null checks, then give line/column to IR member instruction.
    if(builder->compiler->checks & COMPILER_NULL_CHECKS) {
        const char *const buffer = builder->compiler->objects[code_source.object_index]->buffer;
        lex_get_location(buffer, code_source.index, &instruction->maybe_line_number, &instruction->maybe_column_number);
    }

    return build_value_from_prev_instruction(builder);
}

void build_return(ir_builder_t *builder, ir_value_t *value){
    ir_instr_ret_t *instruction = (ir_instr_ret_t*) build_instruction(builder, sizeof(ir_instr_ret_t));

    *instruction = (ir_instr_ret_t){
        .id = INSTRUCTION_RET,
        .result_type = NULL,
        .value = value,
    };
}

ir_value_t *build_static_struct(ir_module_t *module, ir_type_t *type, ir_value_t **values, length_t length, bool make_mutable){
    ir_value_t *value = ir_pool_alloc_init(&module->pool, ir_value_t, {
        .value_type = VALUE_TYPE_STRUCT_LITERAL,
        .type = type,
        .extra = ir_pool_alloc_init(&module->pool, ir_value_struct_literal_t, {
            .values = values,
            .length = length,
        })
    });

    // If mutability is required, then create an anonymous global for the structure data
    if(make_mutable){
        // Create anonymous global
        ir_value_t *anonymous_global_variable = build_anon_global(module, type, true);

        // Set initializer for anonymous global
        build_anon_global_initializer(module, anonymous_global_variable, value);

        // Use mutable anonymous global instead of constant
        value = anonymous_global_variable;
    }

    return value;
}

ir_value_t *build_struct_construction(ir_pool_t *pool, ir_type_t *type, ir_value_t **values, length_t length){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_STRUCT_CONSTRUCTION,
        .type = type,
        .extra = ir_pool_alloc_init(pool, ir_value_struct_construction_t, {
            .values = values,
            .length = length,
        })
    });
}

ir_value_t *build_offsetof(ir_builder_t *builder, ir_type_t *type, length_t index){
    return build_offsetof_ex(builder->pool, ir_builder_usize(builder), type, index);
}

ir_value_t *build_offsetof_ex(ir_pool_t *pool, ir_type_t *usize_type, ir_type_t *type, length_t index){
    if(type->kind != TYPE_KIND_STRUCTURE){
        die("build_offsetof() - Cannot get offset of field for non-struct type\n");
    }

    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_OFFSETOF,
        .type = usize_type,
        .extra = ir_pool_alloc_init(pool, ir_value_offsetof_t, {
            .type = type,
            .index = index,
        })
    });
}

ir_value_t *build_const_sizeof(ir_pool_t *pool, ir_type_t *usize_type, ir_type_t *type){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_CONST_SIZEOF,
        .type = usize_type,
        .extra = ir_pool_alloc_init(pool, ir_value_const_sizeof_t, {
            .type = type,
        })
    });
}

ir_value_t *build_const_alignof(ir_pool_t *pool, ir_type_t *usize_type, ir_type_t *type){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_CONST_ALIGNOF,
        .type = usize_type,
        .extra = ir_pool_alloc_init(pool, ir_value_const_alignof_t, {
            .type = type,
        })
    });
}

ir_value_t *build_const_add(ir_pool_t *pool, ir_value_t *a, ir_value_t *b){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_CONST_ADD,
        .type = a->type,
        .extra = ir_pool_alloc_init(pool, ir_value_const_math_t, {
            .a = a,
            .b = b,
        })
    });
}

ir_value_t *build_static_array(ir_pool_t *pool, ir_type_t *item_type, ir_value_t **values, length_t length){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_ARRAY_LITERAL,
        .type = ir_type_make_pointer_to(pool, item_type),
        .extra = ir_pool_alloc_init(pool, ir_value_array_literal_t, {
            .values = values,
            .length = length,
        })
    });
}

ir_value_t *build_anon_global(ir_module_t *module, ir_type_t *type, bool is_constant){
    unsigned int value_type;
    trait_t traits;

    if(is_constant){
        value_type = VALUE_TYPE_CONST_ANON_GLOBAL;
        traits = IR_ANON_GLOBAL_CONSTANT;
    } else {
        value_type = VALUE_TYPE_ANON_GLOBAL;
        traits = TRAIT_NONE;
    }

    ir_anon_globals_append(&module->anon_globals, (
        (ir_anon_global_t){
            .type = type,
            .traits = traits,
            .initializer = NULL,
        }
    ));

    length_t anon_global_id = module->anon_globals.length - 1;

    return ir_pool_alloc_init(&module->pool, ir_value_t, {
        .value_type = value_type,
        .type = ir_type_make_pointer_to(&module->pool, type),
        .extra = ir_pool_alloc_init(&module->pool, ir_value_anon_global_t, {
            .anon_global_id = anon_global_id,
        })
    });
}

void build_anon_global_initializer(ir_module_t *module, ir_value_t *anon_global, ir_value_t *initializer){
    switch(anon_global->value_type){
    case VALUE_TYPE_ANON_GLOBAL:
    case VALUE_TYPE_CONST_ANON_GLOBAL: {
            ir_value_anon_global_t *extra = (ir_value_anon_global_t*) anon_global->extra;
            module->anon_globals.globals[extra->anon_global_id].initializer = initializer;
        }
        break;
    default:
        internalerrorprintf("build_anon_global_initializer() - Cannot set initializer on a value that isn't a reference to an anonymous global variable\n");
    }
}

ir_type_t* ir_builder_usize(ir_builder_t *builder){
    return builder->object->ir_module.common.ir_usize;
}

ir_type_t* ir_builder_usize_ptr(ir_builder_t *builder){
    return builder->object->ir_module.common.ir_usize_ptr;
}

ir_type_t* ir_builder_bool(ir_builder_t *builder){
    return builder->object->ir_module.common.ir_bool;
}

maybe_index_t ir_builder___types__(ir_builder_t *builder, source_t source_on_failure){
    if(builder->compiler->traits & COMPILER_NO_TYPEINFO){
        compiler_panic(builder->compiler, source_on_failure, "Runtime type information cannot be disabled because of this");
        return -1;
    }

    ir_module_t *ir_module = &builder->object->ir_module;

    switch(ir_module->common.has_rtti_array){
    case TROOLEAN_TRUE:
        return ir_module->common.rtti_array_index;
    case TROOLEAN_FALSE:
        compiler_panic(builder->compiler, source_on_failure, "Failed to find __types__ global variable");
        return -1;
    case TROOLEAN_UNKNOWN:
        // TODO: SPEED: It works, but the global variable lookup could be faster
        for(length_t index = 0; index != ir_module->globals_length; index++){
            if(streq("__types__", ir_module->globals[index].name)){
                ir_module->common.rtti_array_index = index;
                ir_module->common.has_rtti_array = TROOLEAN_TRUE;
                return index;
            }
        }
        break;
    }

    ir_module->common.has_rtti_array = TROOLEAN_FALSE;
    compiler_panic(builder->compiler, source_on_failure, "Failed to find __types__ global variable");
    return -1;
}

ir_value_t *build_literal_int(ir_pool_t *pool, adept_int value){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_LITERAL,
        .type = ir_type_make(pool, TYPE_KIND_S32, NULL),
        .extra = ir_pool_memclone(pool, &value, sizeof value),
    });
}

ir_value_t *build_literal_usize(ir_pool_t *pool, adept_usize value){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_LITERAL,
        .type = ir_type_make(pool, TYPE_KIND_U64, NULL),
        .extra = ir_pool_memclone(pool, &value, sizeof value),
    });
}

ir_value_t *build_unknown_enum_value(ir_pool_t *pool, source_t source, weak_cstr_t kind_name){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_UNKNOWN_ENUM,
        .type = ir_type_make_unknown_enum(pool, source, kind_name),
        .extra = NULL,
    });
}

ir_value_t *build_literal_str(ir_builder_t *builder, char *array, length_t length){
    ir_type_t *ir_string_type = builder->object->ir_module.common.ir_string_struct;

    if(ir_string_type == NULL){
        redprintf("Can't create string literal without String type present");
        printf("\nTry importing '%s/String.adept'\n", ADEPT_VERSION_STRING);
        return NULL;
    }

    length_t num_fields = 4;
    ir_value_t **values = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * num_fields);
    values[0] = build_literal_cstr_of_length(builder, array, length);
    values[1] = build_literal_usize(builder->pool, length);
    values[2] = build_literal_usize(builder->pool, length);
    values[3] = build_literal_usize(builder->pool, 0); // DANGEROUS: This is a hack to initialize String.ownership as a reference

    return ir_pool_alloc_init(builder->pool, ir_value_t, {
        .value_type = VALUE_TYPE_STRUCT_LITERAL,
        .type = ir_string_type,
        .extra = ir_pool_alloc_init(builder->pool, ir_value_struct_literal_t, {
            .values = values,
            .length = num_fields,
        })
    });
}

ir_value_t *build_literal_cstr(ir_builder_t *builder, weak_cstr_t value){
    return build_literal_cstr_of_length(builder, value, strlen(value) + 1);
}

ir_value_t *build_literal_cstr_ex(ir_pool_t *pool, ir_type_map_t *type_map, weak_cstr_t value){
    return build_literal_cstr_of_length_ex(pool, type_map, value, strlen(value) + 1);
}

ir_value_t *build_literal_cstr_of_length(ir_builder_t *builder, char *array, length_t length){
    return build_literal_cstr_of_length_ex(builder->pool, builder->type_map, array, length);
}

ir_value_t *build_literal_cstr_of_length_ex(ir_pool_t *pool, ir_type_map_t *type_map, char *array, length_t size){
    ir_type_t *ir_ubyte_type;

    if(!ir_type_map_find(type_map, "ubyte", &ir_ubyte_type)){
        die("build_literal_cstr_of_length_ex() - Failed to find 'ubyte' type mapping\n");
    }

    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_CSTR_OF_LEN,
        .type = ir_type_make_pointer_to(pool, ir_ubyte_type),
        .extra = ir_pool_alloc_init(pool, ir_value_cstr_of_len_t, {
            .array = array,
            .size = size,
        })
    });
}

ir_value_t *build_null_pointer(ir_pool_t *pool){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_NULLPTR,
        .type = ir_type_make_pointer_to(pool, ir_type_make(pool, TYPE_KIND_S8, NULL)),
        .extra = NULL,
    });
}

ir_value_t *build_null_pointer_of_type(ir_pool_t *pool, ir_type_t *type){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_NULLPTR_OF_TYPE,
        .type = type,
        .extra = NULL,
    });
}

ir_value_t *build_func_addr(ir_pool_t *pool, ir_type_t *result_type, func_id_t ir_func_id){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_FUNC_ADDR,
        .type = result_type,
        .extra = ir_pool_alloc_init(pool, ir_value_func_addr_t, {
            .ir_func_id = ir_func_id,
        })
    });
}

ir_value_t *build_func_addr_by_name(ir_pool_t *pool, ir_type_t *result_type, const char *name){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_FUNC_ADDR_BY_NAME,
        .type = result_type,
        .extra = ir_pool_alloc_init(pool, ir_value_func_addr_by_name_t, {
            .name = name,
        })
    });
}

ir_value_t *build_cast(ir_builder_t *builder, unsigned int const_cast_value_type, unsigned int nonconst_cast_instr_type, ir_value_t *from, ir_type_t *to){
    if(VALUE_TYPE_IS_CONSTANT(from->value_type)){
        return build_const_cast(builder->pool, const_cast_value_type, from, to);
    } else {
        return build_nonconst_cast(builder, nonconst_cast_instr_type, from, to);
    }
}

ir_value_t *build_nonconst_cast(ir_builder_t *builder, unsigned int cast_instr_id, ir_value_t *from, ir_type_t* to){
    ir_instr_cast_t *instruction = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));

    *instruction = (ir_instr_cast_t){
        .id = cast_instr_id,
        .result_type = to,
        .value = from,
    };

    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_const_cast(ir_pool_t *pool, unsigned int cast_value_type, ir_value_t *from, ir_type_t *to){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = cast_value_type,
        .type = to,
        .extra = from,
    });
}

ir_value_t *build_alloc(ir_builder_t *builder, ir_type_t *type){
    ir_instr_alloc_t *instruction = (ir_instr_alloc_t*) build_instruction(builder, sizeof(ir_instr_alloc_t));

    *instruction = (ir_instr_alloc_t){
        .id = INSTRUCTION_ALLOC,
        .result_type = ir_type_make_pointer_to(builder->pool, type),
        .alignment = 0,
        .count = NULL,
    };

    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_alloc_array(ir_builder_t *builder, ir_type_t *type, ir_value_t *count){
    ir_instr_alloc_t *instruction = (ir_instr_alloc_t*) build_instruction(builder, sizeof(ir_instr_alloc_t));

    *instruction = (ir_instr_alloc_t){
        .id = INSTRUCTION_ALLOC,
        .result_type = ir_type_make_pointer_to(builder->pool, type),
        .alignment = 0,
        .count = count,
    };

    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_alloc_aligned(ir_builder_t *builder, ir_type_t *type, unsigned int alignment){
    ir_instr_alloc_t *instruction = (ir_instr_alloc_t*) build_instruction(builder, sizeof(ir_instr_alloc_t));

    *instruction = (ir_instr_alloc_t){
        .id = INSTRUCTION_ALLOC,
        .result_type = ir_type_make_pointer_to(builder->pool, type),
        .alignment = alignment,
        .count = NULL,
    };

    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_stack_save(ir_builder_t *builder){
    ir_instr_t *instruction = (ir_instr_t*) build_instruction(builder, sizeof(ir_instr_t));

    *instruction = (ir_instr_t){
        .id = INSTRUCTION_STACK_SAVE,
        .result_type = builder->object->ir_module.common.ir_ptr,
    };

    return build_value_from_prev_instruction(builder);
}

void build_stack_restore(ir_builder_t *builder, ir_value_t *stack_pointer){
    ir_instr_unary_t *instruction = (ir_instr_unary_t*) build_instruction(builder, sizeof(ir_instr_unary_t));

    *instruction = (ir_instr_unary_t){
        .id = INSTRUCTION_STACK_RESTORE,
        .result_type = NULL,
        .value = stack_pointer,
    };
}

ir_value_t *build_math(ir_builder_t *builder, unsigned int instr_id, ir_value_t *a, ir_value_t *b, ir_type_t *result){
    ir_instr_math_t *instruction = (ir_instr_math_t*) build_instruction(builder, sizeof(ir_instr_math_t));

    *instruction = (ir_instr_math_t){
        .id = instr_id,
        .a = a,
        .b = b,
        .result_type = result,
    };

    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_phi2(ir_builder_t *builder, ir_type_t *result_type, ir_value_t *a, ir_value_t *b, length_t landing_a_block_id, length_t landing_b_block_id){
    ir_instr_phi2_t *instruction = (ir_instr_phi2_t*) build_instruction(builder, sizeof(ir_instr_phi2_t));

    *instruction = (ir_instr_phi2_t){
        .id = INSTRUCTION_PHI2,
        .result_type = result_type,
        .a = a,
        .b = b,
        .block_id_a = landing_a_block_id,
        .block_id_b = landing_b_block_id,
    };

    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_bool(ir_pool_t *pool, adept_bool value){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_LITERAL,
        .type = ir_type_make(pool, TYPE_KIND_BOOLEAN, NULL),
        .extra = ir_pool_memclone(pool, &value, sizeof value),
    });
}

errorcode_t build_rtti_relocation(ir_builder_t *builder, strong_cstr_t human_notation, adept_usize *id_ref, source_t source_on_failure){
    rtti_relocations_append(&builder->object->ir_module.rtti_relocations, (
        (rtti_relocation_t){
            .human_notation = human_notation,
            .id_ref = id_ref,
            .source_on_failure = source_on_failure,
        }
    ));

    return SUCCESS;
}

void build_llvm_asm(ir_builder_t *builder, bool is_intel, weak_cstr_t assembly, weak_cstr_t constraints, ir_value_t **args, length_t arity, bool has_side_effects, bool is_stack_align){
    ir_instr_asm_t *instruction = (ir_instr_asm_t*) build_instruction(builder, sizeof(ir_instr_asm_t));

    *instruction = (ir_instr_asm_t){
        .id = INSTRUCTION_ASM,
        .result_type = NULL,
        .assembly = assembly,
        .constraints = constraints,
        .args = args,
        .arity = arity,
        .is_intel = is_intel,
        .has_side_effects = has_side_effects,
        .is_stack_align = is_stack_align,
    };
}

void build_deinit_svars(ir_builder_t *builder){
    ir_instr_t *instruction = build_instruction(builder, sizeof(ir_instr_t));

    *instruction = (ir_instr_t){
        .id = INSTRUCTION_DEINIT_SVARS,
        .result_type = NULL,
    };
}

void build_unreachable(ir_builder_t *builder){
    ir_instr_t *instruction = (ir_instr_t*) build_instruction(builder, sizeof(ir_instr_t));

    *instruction = (ir_instr_t){
        .id = INSTRUCTION_UNREACHABLE,
        .result_type = NULL,
    };
}

void build_main_deinitialization(ir_builder_t *builder){
    handle_deference_for_globals(builder);
    build_deinit_svars(builder);
}

void open_scope(ir_builder_t *builder){
    bridge_scope_t *old_scope = builder->scope;

    bridge_scope_t *new_scope = malloc(sizeof(bridge_scope_t));
    bridge_scope_init(new_scope, old_scope);

    new_scope->first_var_id = builder->next_var_id;

    bridge_scope_ref_list_append(&old_scope->children, new_scope);
    builder->scope = new_scope;
}

void close_scope(ir_builder_t *builder){
    builder->scope->following_var_id = builder->next_var_id;
    builder->scope = builder->scope->parent;
    
    if(builder->scope == NULL){
        die("close_scope() - Cannot close bridge scope with no parent\n");
    }
}

void push_loop_label(ir_builder_t *builder, weak_cstr_t label, length_t break_basicblock_id, length_t continue_basicblock_id){
    block_stack_push(&builder->block_stack, ((block_t){
        .label = label,
        .break_id = break_basicblock_id,
        .continue_id = continue_basicblock_id,
        .scope = builder->scope,
    }));
}

void pop_loop_label(ir_builder_t *builder){
    block_stack_pop(&builder->block_stack);
}

bridge_var_t *add_variable(ir_builder_t *builder, weak_cstr_t name, ast_type_t *ast_type, ir_type_t *ir_type, trait_t traits){
    bridge_var_list_t *list = &builder->scope->list;

    index_id_t id = INVALID_INDEX_ID;
    index_id_t static_id = INVALID_INDEX_ID;

    if(traits & BRIDGE_VAR_STATIC){
        ir_static_variables_t *static_variables = &builder->object->ir_module.static_variables;

        static_id = static_variables->length;

        // Create ir_static_variable_t
        ir_static_variables_append(static_variables, ((ir_static_variable_t){
            .type = ir_type,
        }));
    } else {
        id = builder->next_var_id++;
    }

    bridge_var_list_append(list, ((bridge_var_t){
        .name = name,
        .ast_type = ast_type,
        .traits = traits,
        .ir_type = ir_type,
        .id = id,
        .static_id = static_id,
    }));

    return &list->variables[list->length - 1];
}

errorcode_t handle_deference_for_variables(ir_builder_t *builder, bridge_var_list_t *list){
    for(length_t i = 0; i != list->length; i++){
        bridge_var_t *variable = &list->variables[i];

        trait_t traits = variable->traits;
        unsigned int ir_type_kind = variable->ir_type->kind;

        if(traits & (BRIDGE_VAR_POD | BRIDGE_VAR_REFERENCE) || !(ir_type_kind == TYPE_KIND_STRUCTURE && ir_type_kind != TYPE_KIND_FIXED_ARRAY)){
            continue;
        }

        ir_builder_t *defer_builder = traits & BRIDGE_VAR_STATIC ? builder->object->ir_module.deinit_builder : builder;

        ir_pool_snapshot_t pool_snapshot = ir_pool_snapshot_capture(defer_builder->pool);
        instructions_snapshot_t instructions_snapshot = instructions_snapshot_capture(builder);

        ir_value_t *ir_variable_value = build_varptr(defer_builder, ir_type_make_pointer_to(defer_builder->pool, variable->ir_type), variable);
        errorcode_t failed = handle_single_deference(defer_builder, variable->ast_type, ir_variable_value, variable->source);

        if(failed){
            instructions_snapshot_restore(builder, &instructions_snapshot);
            ir_pool_snapshot_restore(defer_builder->pool, &pool_snapshot);

            // Real failure if a compile time error occurred
            if(failed == ALT_FAILURE) return FAILURE;
        }
    }

    return SUCCESS;
}

errorcode_t handle_deference_for_globals(ir_builder_t *builder){
    ast_global_t *globals = builder->object->ast.globals;
    length_t globals_length = builder->object->ast.globals_length;

    for(length_t i = 0; i != globals_length; i++){
        ast_global_t *global = &globals[i];

        // Don't perform defer management on POD variables or non-struct types or special globals
        trait_t traits = global->traits;
        
        // Skip deference for global variables with any of these traits
        if(traits & (AST_GLOBAL_EXTERNAL | AST_GLOBAL_POD | AST_GLOBAL_SPECIAL)) continue;

        // Ensure we're working with a single AST type element in the type
        if(global->type.elements_length != 1) continue;

        // Capture snapshot of current pool state (for if we need to revert allocations)
        ir_pool_snapshot_t pool_snapshot = ir_pool_snapshot_capture(builder->pool);

        ir_type_t *ir_type;
        if(ir_gen_resolve_type(builder->compiler, builder->object, &global->type, &ir_type)){
            // Revert recent pool allocations
            ir_pool_snapshot_restore(builder->pool, &pool_snapshot);
            return FAILURE;
        }

        // Capture snapshot of current instruction building state (for if we need to revert)
        instructions_snapshot_t instructions_snapshot = instructions_snapshot_capture(builder);

        // DANGEROUS: Assuming i is the same as the global variable id
        ir_value_t *ir_value = build_gvarptr(builder, ir_type, i);
        errorcode_t failed = handle_single_deference(builder, &global->type, ir_value, global->source);

        if(failed){
            instructions_snapshot_restore(builder, &instructions_snapshot);

            // Revert recent pool allocations
            ir_pool_snapshot_restore(builder->pool, &pool_snapshot);

            // Real failure if a compile time error occurred
            if(failed == ALT_FAILURE) return FAILURE;
        }
    }
    return SUCCESS;
}

static errorcode_t visit_fixed_array_items(
    ir_builder_t *builder,
    ast_type_t *ast_type,
    ir_value_t *mutable_value,
    source_t from_source,
    bool (*could_have)(ast_type_t*),
    errorcode_t (*handle_single)(ir_builder_t*, ast_type_t*, ir_value_t*, source_t)
){
    ast_type_t temporary_rest_of_type = ast_type_unwrapped_view(ast_type);

    if(!could_have(&temporary_rest_of_type)) return FAILURE;

    // Record the number of items of the fixed array
    length_t count = ((ast_elem_fixed_array_t*) ast_type->elements[0])->length;

    // Remember current state of instruction building in case we have to revert back
    ir_pool_snapshot_t pool_snapshot = ir_pool_snapshot_capture(builder->pool);
    instructions_snapshot_t instructions_snapshot = instructions_snapshot_capture(builder);

    // Convert from '*n T' to '*T'
    mutable_value = build_array_access(builder, mutable_value, build_literal_usize(builder->pool, 0), ast_type->source);

    // TODO: Make this a runtime loop if count is big enough
    for(length_t i = 0; i != count; i++){
        // Call handle_single_dereference() on each item else restore snapshots

        // Access at 'i'
        ir_value_t *mutable_item_value = build_array_access(builder, mutable_value, build_literal_usize(builder->pool, i), ast_type->source);
        
        // Handle deference for that single item
        errorcode_t errorcode = handle_single(builder, &temporary_rest_of_type, mutable_item_value, from_source);

        if(errorcode){
            instructions_snapshot_restore(builder, &instructions_snapshot);
            ir_pool_snapshot_restore(builder->pool, &pool_snapshot);
            return errorcode;
        }
    }

    return SUCCESS;
}

errorcode_t handle_single_deference(ir_builder_t *builder, ast_type_t *ast_type, ir_value_t *mutable_value, source_t from_source){
    // Calls __defer__ method on a mutable value and it's children if the method exists
    // NOTE: Assumes (ast_type->elements_length >= 1)
    // NOTE: Returns SUCCESS if mutable_value was utilized in deference
    //       Returns FAILURE if mutable_value was not utilized in deference
    //       Returns ALT_FAILURE if a compiler time error occurred
    // NOTE: This function is not allowed to generate or switch basicblocks!

    switch(ast_type->elements[0]->id){
    case AST_ELEM_BASE:
    case AST_ELEM_GENERIC_BASE: {
            ir_pool_snapshot_t pool_snapshot = ir_pool_snapshot_capture(builder->pool);

            optional_func_pair_t pair;
            errorcode_t errorcode = ir_gen_find_defer_func(builder->compiler, builder->object, ast_type, &pair);

            if(errorcode){
                ir_pool_snapshot_restore(builder->pool, &pool_snapshot);
                return errorcode;
            }

            // Call __defer__()
            if(pair.has){
                ir_type_t *result_type = builder->object->ir_module.funcs.funcs[pair.value.ir_func_id].return_type;
                ir_value_t **arguments = ir_pool_alloc_init(builder->pool, ir_value_t*, mutable_value);

                build_call_ignore_result(builder, pair.value.ir_func_id, result_type, arguments, 1);
                return SUCCESS;
            }

            return FAILURE;
        }
    case AST_ELEM_FIXED_ARRAY:
        return visit_fixed_array_items(builder, ast_type, mutable_value, from_source, could_have_deference, handle_single_deference);
    default:
        // value.__defer__() is a noop
        // Return FAILURE, since we didn't utilize given value
        return FAILURE;
    }
}

errorcode_t handle_children_deference(ir_builder_t *builder){
    // Generates __defer__ calls for children of the parent type of a __defer__ function

    // DANGEROUS: 'func' could be invalidated by generation of new functions
    ast_func_t *func = &builder->object->ast.funcs[builder->ast_func_id];
    ast_type_t *this_ast_type = func->arity == 1 ? &func->arg_types[0] : NULL;

    if(this_ast_type == NULL || this_ast_type->elements_length != 2){
        compiler_panicf(builder->compiler, func->source, "INTERNAL ERROR: handle_children_dereference() encountered unrecognized format of __defer__ management method");
        return FAILURE;
    }

    if(func->arg_type_traits[0] & BRIDGE_VAR_POD) return SUCCESS;

    ast_elem_t *concrete_elem = this_ast_type->elements[1];

    if(this_ast_type->elements[0]->id != AST_ELEM_POINTER || !(concrete_elem->id == AST_ELEM_BASE || concrete_elem->id == AST_ELEM_GENERIC_BASE)){
        compiler_panicf(builder->compiler, func->source, "INTERNAL ERROR: handle_children_dereference() encountered unrecognized argument types of __defer__ management method");
        return FAILURE;
    }

    switch(concrete_elem->id){
    case AST_ELEM_BASE: {
            weak_cstr_t struct_name = ((ast_elem_base_t*) concrete_elem)->base;

            // Attempt to call __defer__ on members
            ast_composite_t *composite = ast_composite_find_exact(&builder->object->ast, struct_name);

            // Don't bother with structs that don't exist
            if(composite == NULL) return FAILURE;

            // Don't handle children for complex composite types
            if(!ast_layout_is_simple_struct(&composite->layout)) return SUCCESS;

            ast_layout_skeleton_t *skeleton = &composite->layout.skeleton;
            length_t field_count = ast_simple_field_map_get_count(&composite->layout.field_map);

            for(length_t f = 0; f != field_count; f++){
                ast_type_t *ast_field_type = ast_layout_skeleton_get_type_at_index(skeleton, f);

                // Capture snapshot for if we need to backtrack
                ir_pool_snapshot_t snapshot = ir_pool_snapshot_capture(builder->pool);
                
                ir_type_t *ir_field_type, *this_ir_type;
                if(
                    ir_gen_resolve_type(builder->compiler, builder->object, ast_field_type, &ir_field_type) ||
                    ir_gen_resolve_type(builder->compiler, builder->object, this_ast_type, &this_ir_type)
                ){
                    return ALT_FAILURE;
                }

                ir_value_t *this_ir_value = build_load(builder, build_lvarptr(builder, ir_type_make_pointer_to(builder->pool, this_ir_type), 0), this_ast_type->source);

                ir_value_t *ir_field_value = build_member(builder, this_ir_value, f, ir_type_make_pointer_to(builder->pool, ir_field_type), this_ast_type->source);
                errorcode_t failed = handle_single_deference(builder, ast_field_type, ir_field_value, composite->source);

                if(failed){
                    // Remove VARPTR, LOAD, and MEMBER instruction
                    builder->current_block->instructions.length -= 3;

                    // Revert recent pool allocations
                    ir_pool_snapshot_restore(builder->pool, &snapshot);

                    // Propagate alternate failure cause
                    if(failed == ALT_FAILURE) return ALT_FAILURE;
                }
            }
        }
        break;
    case AST_ELEM_GENERIC_BASE: {
            ast_elem_generic_base_t *generic_base = (ast_elem_generic_base_t*) concrete_elem;
            weak_cstr_t template_name = generic_base->name;

            // Attempt to call __defer__ on members
            ast_poly_composite_t *template = ast_poly_composite_find_exact(&builder->object->ast, template_name);

            // Don't bother with polymorphic structs that don't exist
            if(template == NULL) return FAILURE;

            // Don't handle children for complex composite types
            if(!ast_layout_is_simple_struct(&template->layout)) return SUCCESS;

            // Substitution Catalog
            ast_poly_catalog_t catalog;
            ast_poly_catalog_init(&catalog);

            if(template->generics_length != generic_base->generics_length){
                internalerrorprintf("handle_children_dereference() - Polymorphic struct '%s' type parameter length mismatch when generating child deference!\n", generic_base->name);
                ast_poly_catalog_free(&catalog);
                return ALT_FAILURE;
            }

            for(length_t i = 0; i != template->generics_length; i++){
                ast_poly_catalog_add_type(&catalog, template->generics[i], &generic_base->generics[i]);
            }

            ast_layout_skeleton_t *skeleton = &template->layout.skeleton;
            length_t field_count = ast_simple_field_map_get_count(&template->layout.field_map);

            for(length_t f = 0; f != field_count; f++){
                ast_type_t *ast_unresolved_field_type = ast_layout_skeleton_get_type_at_index(skeleton, f);
                ast_type_t ast_field_type;

                if(ast_resolve_type_polymorphs(builder->compiler, builder->type_table, &catalog, ast_unresolved_field_type, &ast_field_type)){
                    ast_poly_catalog_free(&catalog);
                    return ALT_FAILURE;
                }

                // Capture snapshot for if we need to backtrack
                ir_pool_snapshot_t snapshot = ir_pool_snapshot_capture(builder->pool);
                
                ir_type_t *ir_field_type, *this_ir_type;
                if(
                    ir_gen_resolve_type(builder->compiler, builder->object, &ast_field_type, &ir_field_type) ||
                    ir_gen_resolve_type(builder->compiler, builder->object, this_ast_type, &this_ir_type)
                ){
                    ast_poly_catalog_free(&catalog);
                    ast_type_free(&ast_field_type);
                    return ALT_FAILURE;
                }

                ir_value_t *this_ir_value = build_load(builder, build_lvarptr(builder, ir_type_make_pointer_to(builder->pool, this_ir_type), 0), this_ast_type->source);

                ir_value_t *ir_field_value = build_member(builder, this_ir_value, f, ir_type_make_pointer_to(builder->pool, ir_field_type), this_ast_type->source);
                errorcode_t failed = handle_single_deference(builder, &ast_field_type, ir_field_value, template->source);
                ast_type_free(&ast_field_type);

                if(failed){
                    // Remove VARPTR, LOAD, and MEMBER instruction
                    builder->current_block->instructions.length -= 3;

                    // Revert recent pool allocations
                    ir_pool_snapshot_restore(builder->pool, &snapshot);

                    // Propagate alternate failure cause
                    if(failed == ALT_FAILURE){
                        ast_poly_catalog_free(&catalog);
                        return ALT_FAILURE;
                    }
                }
            }

            ast_poly_catalog_free(&catalog);
        }
        break;
    default:
        return FAILURE;
    }

    return SUCCESS;
}

bool could_have_deference(ast_type_t *ast_type){
    switch(ast_type->elements[0]->id){
    case AST_ELEM_BASE:
    case AST_ELEM_GENERIC_BASE:
    case AST_ELEM_FIXED_ARRAY:
        return true;
    }
    return false;
}

static inline bool ast_type_allows_pass_management(ast_type_t *type){
    if(type->elements_length != 1) return false;

    switch(type->elements[0]->id){
    case AST_ELEM_BASE:
    case AST_ELEM_GENERIC_BASE:
    case AST_ELEM_FIXED_ARRAY:
        return true;
    default:
        return false;
    }
}

errorcode_t handle_pass_management(ir_builder_t *builder, ir_value_t **values, ast_type_t *types, trait_t *arg_type_traits, length_t arity){
    for(length_t i = 0; i != arity; i++){
        // Don't do __pass__ calls for parameters marked as 'POD'
        if(arg_type_traits != NULL && arg_type_traits[i] & AST_FUNC_ARG_TYPE_TRAIT_POD) continue;
        
        unsigned int value_type_kind = values[i]->type->kind;

        if(value_type_kind == TYPE_KIND_STRUCTURE || value_type_kind == TYPE_KIND_FIXED_ARRAY){
            if(ast_type_allows_pass_management(&types[i])){
                optional_func_pair_t pair;
                errorcode_t errorcode = ir_gen_find_pass_func(builder->compiler, builder->object, &types[i], &pair);

                if(errorcode == ALT_FAILURE){
                    return FAILURE;
                } else if(errorcode){
                    continue;
                }

                if(pair.has){
                    func_id_t ir_func_id = pair.value.ir_func_id;

                    ir_value_t **arguments = ir_pool_alloc_init(builder->pool, ir_value_t*, values[i]);
                    ir_type_t *return_type = builder->object->ir_module.funcs.funcs[ir_func_id].return_type;                

                    values[i] = build_call(builder, ir_func_id, return_type, arguments, 1);
                }
            }
        }
    }

    return SUCCESS;
}

errorcode_t handle_single_pass(ir_builder_t *builder, ast_type_t *ast_type, ir_value_t *mutable_value, source_t from_source){
    // Calls __pass__ method on a mutable value and it's children if the method exists
    // NOTE: Assumes (ast_type->elements_length >= 1)
    // NOTE: Returns SUCCESS if mutable_value was utilized in deference
    //       Returns FAILURE if mutable_value was not utilized in deference
    //       Returns ALT_FAILURE if a compiler time error occurred
    // NOTE: This function is not allowed to generate or switch basicblocks!

    switch(ast_type->elements[0]->id){
    case AST_ELEM_BASE:
    case AST_ELEM_GENERIC_BASE: {
            optional_func_pair_t pair;
            errorcode_t errorcode = ir_gen_find_pass_func(builder->compiler, builder->object, ast_type, &pair);

            if(errorcode){
                return errorcode;
            }

            // Has a __pass__ implementation?
            if(pair.has){
                func_id_t ir_func_id = pair.value.ir_func_id;

                ir_value_t **arguments = ir_pool_alloc_init(builder->pool, ir_value_t*, build_load(builder, mutable_value, ast_type->source));
                ir_type_t *result_type = builder->object->ir_module.funcs.funcs[ir_func_id].return_type;

                // Perform __pass__
                build_store(builder, build_call(builder, ir_func_id, result_type, arguments, 1), mutable_value, ast_type->source);
                return SUCCESS;
            }

            return FAILURE;
        }
    case AST_ELEM_FIXED_ARRAY:
        return visit_fixed_array_items(builder, ast_type, mutable_value, from_source, could_have_pass, handle_single_pass);
    default:
        // __pass__() for this type is a noop
        // Return FAILURE since we didn't utilize given value
        return FAILURE;
    }
}

errorcode_t handle_children_pass_root(ir_builder_t *builder, bool already_has_return){
    errorcode_t res = handle_children_pass(builder);
    if(res) return res;

    // DANGEROUS: 'func' could be invalidated by generation of new functions
    ast_func_t *autogen_func = &builder->object->ast.funcs[builder->ast_func_id];

    ast_type_t *passed_ast_type = autogen_func->arity == 1 ? &autogen_func->arg_types[0] : NULL;
    if(passed_ast_type == NULL) return FAILURE;

    ir_type_t *passed_ir_type;
    if(ir_gen_resolve_type(builder->compiler, builder->object, passed_ast_type, &passed_ir_type)){
        return FAILURE;
    }

    ir_value_t *variable_reference = build_lvarptr(builder, ir_type_make_pointer_to(builder->pool, passed_ir_type), 0);
    ir_value_t *return_value = build_load(builder, variable_reference, passed_ast_type->source);

    // Return modified value
    if(!already_has_return){
        build_return(builder, return_value);
    }

    return SUCCESS;
}

errorcode_t handle_children_pass(ir_builder_t *builder){
    // Generates __pass__ calls for children of the parent type of a __pass__ function

    // DANGEROUS: 'func' could be invalidated by generation of new functions
    ast_func_t *func = &builder->object->ast.funcs[builder->ast_func_id];
    ast_type_t *passed_ast_type = func->arity == 1 ? &func->arg_types[0] : NULL;

    if(passed_ast_type == NULL || passed_ast_type->elements_length < 1){
        compiler_panicf(builder->compiler, func->source, "INTERNAL ERROR: handle_children_pass() encountered unrecognized format of __pass__ management method");
        return FAILURE;
    }

    // Don't call __pass__ for parameters marked as plain-old data
    if(func->arg_type_traits[0] & BRIDGE_VAR_POD) return SUCCESS;

    ast_elem_t *first_elem = passed_ast_type->elements[0];

    // Generate code for __pass__ function
    switch(first_elem->id){
    case AST_ELEM_BASE: {
            weak_cstr_t struct_name = ((ast_elem_base_t*) first_elem)->base;

            // Attempt to call __pass__ on members
            ast_composite_t *composite = ast_composite_find_exact(&builder->object->ast, struct_name);

            // Don't bother with structs that don't exist
            if(composite == NULL) return FAILURE;

            // Don't handle children for complex composite types
            if(!ast_layout_is_simple_struct(&composite->layout)) return SUCCESS;

            ast_layout_skeleton_t *skeleton = &composite->layout.skeleton;
            length_t field_count = ast_simple_field_map_get_count(&composite->layout.field_map);

            for(length_t f = 0; f != field_count; f++){
                ast_type_t *ast_field_type = ast_layout_skeleton_get_type_at_index(skeleton, f);

                // Capture snapshot for if we need to backtrack
                ir_pool_snapshot_t snapshot = ir_pool_snapshot_capture(builder->pool);
                
                ir_type_t *ir_field_type, *passed_ir_type;
                if(
                    ir_gen_resolve_type(builder->compiler, builder->object, ast_field_type, &ir_field_type) ||
                    ir_gen_resolve_type(builder->compiler, builder->object, passed_ast_type, &passed_ir_type)
                ){
                    return ALT_FAILURE;
                }

                if(ir_field_type->kind != TYPE_KIND_STRUCTURE && ir_field_type->kind != TYPE_KIND_FIXED_ARRAY){
                    ir_pool_snapshot_restore(builder->pool, &snapshot);
                    continue;
                }

                ir_value_t *mutable_passed_ir_value = build_lvarptr(builder, ir_type_make_pointer_to(builder->pool, passed_ir_type), 0);

                ir_value_t *ir_field_reference = build_member(builder, mutable_passed_ir_value, f, ir_type_make_pointer_to(builder->pool, ir_field_type), ast_field_type->source);
                errorcode_t failed = handle_single_pass(builder, ast_field_type, ir_field_reference, NULL_SOURCE);

                if(failed){
                    // Remove VARPTR and MEMBER instruction
                    builder->current_block->instructions.length -= 2;

                    // Revert recent pool allocations
                    ir_pool_snapshot_restore(builder->pool, &snapshot);

                    // Propagate alternate failure cause
                    if(failed == ALT_FAILURE) return ALT_FAILURE;
                }
            }
        }
        break;
    case AST_ELEM_GENERIC_BASE: {
            ast_elem_generic_base_t *generic_base = (ast_elem_generic_base_t*) first_elem;
            weak_cstr_t template_name = generic_base->name;

            // Attempt to call __pass__ on members
            ast_poly_composite_t *template = ast_poly_composite_find_exact(&builder->object->ast, template_name);

            // Don't bother with polymorphic structs that don't exist
            if(template == NULL) return FAILURE;

            // Don't handle children for complex composite types
            if(!ast_layout_is_simple_struct(&template->layout)) return SUCCESS;

            // Substitution Catalog
            ast_poly_catalog_t catalog;
            ast_poly_catalog_init(&catalog);

            if(template->generics_length != generic_base->generics_length){
                internalerrorprintf("handle_children_pass() - Polymorphic struct '%s' type parameter length mismatch when generating child passing!\n", generic_base->name);
                ast_poly_catalog_free(&catalog);
                return ALT_FAILURE;
            }

            for(length_t i = 0; i != template->generics_length; i++){
                ast_poly_catalog_add_type(&catalog, template->generics[i], &generic_base->generics[i]);
            }

            ast_layout_skeleton_t *skeleton = &template->layout.skeleton;
            length_t field_count = ast_simple_field_map_get_count(&template->layout.field_map);

            for(length_t f = 0; f != field_count; f++){
                ast_type_t *ast_unresolved_field_type = ast_layout_skeleton_get_type_at_index(skeleton, f);
                ast_type_t ast_field_type;

                if(ast_resolve_type_polymorphs(builder->compiler, builder->type_table, &catalog, ast_unresolved_field_type, &ast_field_type)){
                    ast_poly_catalog_free(&catalog);
                    return ALT_FAILURE;
                }

                // Capture snapshot for if we need to backtrack
                ir_pool_snapshot_t snapshot = ir_pool_snapshot_capture(builder->pool);

                ir_type_t *ir_field_type, *passed_ir_type;
                if(
                    ir_gen_resolve_type(builder->compiler, builder->object, &ast_field_type, &ir_field_type) ||
                    ir_gen_resolve_type(builder->compiler, builder->object, passed_ast_type, &passed_ir_type)
                ){
                    ast_poly_catalog_free(&catalog);
                    ast_type_free(&ast_field_type);
                    return ALT_FAILURE;
                }

                if(ir_field_type->kind != TYPE_KIND_STRUCTURE && ir_field_type->kind != TYPE_KIND_FIXED_ARRAY){
                    ir_pool_snapshot_restore(builder->pool, &snapshot);
                    ast_type_free(&ast_field_type);
                    continue;
                }

                ir_value_t *mutable_passed_ir_value = build_lvarptr(builder, ir_type_make_pointer_to(builder->pool, passed_ir_type), 0);

                ir_value_t *ir_field_reference = build_member(builder, mutable_passed_ir_value, f, ir_type_make_pointer_to(builder->pool, ir_field_type), ast_field_type.source);
                errorcode_t failed = handle_single_pass(builder, &ast_field_type, ir_field_reference, NULL_SOURCE);
                ast_type_free(&ast_field_type);

                if(failed){
                    // Remove VARPTR, LOAD, and MEMBER instruction
                    builder->current_block->instructions.length -= 3;

                    // Revert recent pool allocations
                    ir_pool_snapshot_restore(builder->pool, &snapshot);

                    // Propagate alternate failure cause
                    if(failed == ALT_FAILURE){
                        ast_poly_catalog_free(&catalog);
                        return ALT_FAILURE;
                    }
                }
            }

            ast_poly_catalog_free(&catalog);
        }
        break;
    case AST_ELEM_FIXED_ARRAY: {
            if(passed_ast_type->elements_length < 2){
                compiler_panic(builder->compiler, passed_ast_type->source, "INTERNAL ERROR: AST_ELEM_FIXED_ARRAY of handle_children_pass only got one element in type");
                return ALT_FAILURE;
            }
            
            // Attempt to call __pass__ on elements
            length_t fixed_count = ((ast_elem_fixed_array_t*) first_elem)->length;

            ast_type_t ast_element_type;
            ast_element_type.elements = &passed_ast_type->elements[1];
            ast_element_type.elements_length = passed_ast_type->elements_length - 1;
            ast_element_type.source = first_elem->source;

            // OPTIMIZATION: SPEED: CLEANUP: Reduce the number of generated instructions for this
            for(length_t e = 0; e != fixed_count; e++){
                // Capture snapshot for if we need to backtrack
                ir_pool_snapshot_t snapshot = ir_pool_snapshot_capture(builder->pool);

                ir_type_t *ir_element_type, *passed_ir_type;
                if(
                    ir_gen_resolve_type(builder->compiler, builder->object, &ast_element_type, &ir_element_type) ||
                    ir_gen_resolve_type(builder->compiler, builder->object, passed_ast_type, &passed_ir_type)
                ){
                    return ALT_FAILURE;
                }

                if(ir_element_type->kind != TYPE_KIND_STRUCTURE && ir_element_type->kind != TYPE_KIND_FIXED_ARRAY){
                    ir_pool_snapshot_restore(builder->pool, &snapshot);
                    continue;
                }

                // Get type of pointer to element
                ir_type_t *ir_element_ptr_type = ir_type_make_pointer_to(builder->pool, ir_element_type);

                // Get pointer to elements of fixed array variable
                ir_value_t *mutable_passed_ir_value = build_lvarptr(builder, ir_type_make_pointer_to(builder->pool, passed_ir_type), 0);
                mutable_passed_ir_value = build_bitcast(builder, mutable_passed_ir_value, ir_element_ptr_type);

                // Access 'e'th element
                ir_value_t *ir_element_reference = build_array_access(builder, mutable_passed_ir_value, build_literal_usize(builder->pool, e), ast_element_type.source);

                // Handle passing for that element
                errorcode_t failed = handle_single_pass(builder, &ast_element_type, ir_element_reference, NULL_SOURCE);

                if(failed){
                    // Remove VARPTR and BITCAST and ARRAY_ACCESS instructions
                    builder->current_block->instructions.length -= 3;

                    // Revert recent pool allocations
                    ir_pool_snapshot_restore(builder->pool, &snapshot);

                    // Propagate alternate failure cause
                    if(failed == ALT_FAILURE) return ALT_FAILURE;
                }
            }
        }
        break;
    default:
        compiler_panicf(builder->compiler, func->source, "INTERNAL ERROR: handle_children_pass() encountered unrecognized argument types of __pass__ management method");
        return FAILURE;
    }

    return SUCCESS;
}

bool could_have_pass(ast_type_t *ast_type){
    switch(ast_type->elements[0]->id){
    case AST_ELEM_BASE:
    case AST_ELEM_GENERIC_BASE:
    case AST_ELEM_FIXED_ARRAY:
        return true;
    default:
        return false;
    }
}

errorcode_t handle_assign_management(
    ir_builder_t *builder,
    ir_value_t *value,
    ast_type_t *value_ast_type,
    ir_value_t *destination,
    ast_type_t *destination_ast_type,
    source_t source_on_failure
){
    // Ensure value is a structure value
    if(value->type->kind != TYPE_KIND_STRUCTURE || value_ast_type->elements_length != 1) return FAILURE;

    optional_func_pair_t result;
    errorcode_t errorcode;

    ir_pool_snapshot_t pool_snapshot = ir_pool_snapshot_capture(builder->pool);
    instructions_snapshot_t instructions_snapshot = instructions_snapshot_capture(builder);

    errorcode = ir_gen_find_assign_func(builder->compiler, builder->object, destination_ast_type, &result);
    if(errorcode) goto failure;

    if(result.has){
        ast_t *ast = &builder->object->ast;
        func_pair_t pair = result.value;

        if(ast->funcs[pair.ast_func_id].traits & AST_FUNC_DISALLOW){
            strong_cstr_t typename = ast_type_str(value_ast_type);
            compiler_panicf(builder->compiler, source_on_failure, "Assignment for type '%s' is not allowed", typename);
            free(typename);

            errorcode = ALT_FAILURE;
            goto failure;
        }

        build_zeroinit(builder, destination);

        ast_type_t arg_types[2] = {
            ast_type_pointer_to(ast_type_clone(destination_ast_type)),
            *destination_ast_type,
        };

        if(!ast_types_conform(builder, &value, value_ast_type, destination_ast_type, CONFORM_MODE_CALL_ARGUMENTS_LOOSE)){
            goto failure;
        }

        ir_value_t **arguments = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * 2);
        arguments[0] = destination;
        arguments[1] = value;

        errorcode = handle_pass_management(builder, arguments, ast->funcs[pair.ast_func_id].arg_types, ast->funcs[pair.ast_func_id].arg_type_traits, 2);
        ast_type_free(&arg_types[0]);

        if(errorcode) goto failure;
    
        ir_type_t *result_type = builder->object->ir_module.funcs.funcs[pair.ir_func_id].return_type;
        build_call_ignore_result(builder, pair.ir_func_id, result_type, arguments, 2);
        return SUCCESS;
    }

    errorcode = FAILURE;

failure:
    instructions_snapshot_restore(builder, &instructions_snapshot);
    ir_pool_snapshot_restore(builder->pool, &pool_snapshot);
    return errorcode;
}

ir_value_t *handle_math_management(ir_builder_t *builder, ir_math_operands_t *ops, source_t from_source, ast_type_t *out_type, weak_cstr_t overload_name){
    if(ops->lhs->type->kind != TYPE_KIND_STRUCTURE) return NULL;

    if(ast_type_is_base(ops->lhs_type)){
        ir_pool_snapshot_t snapshot = ir_pool_snapshot_capture(builder->pool);

        ir_value_t **arguments = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * 2);
        arguments[0] = ops->lhs;
        arguments[1] = ops->rhs;

        ast_type_t types[2] = {*ops->lhs_type, *ops->rhs_type};

        ast_t *ast = &builder->object->ast;
        ir_module_t *module = &builder->object->ir_module;
        
        optional_func_pair_t result;

        if(ir_gen_find_func_conforming_without_defaults(builder, overload_name, arguments, types, 2, NULL, false, from_source, &result)
        || !result.has
        || handle_pass_management(builder, arguments, ast->funcs[result.value.ast_func_id].arg_types, ast->funcs[result.value.ast_func_id].arg_type_traits, 2)){
            ir_pool_snapshot_restore(builder->pool, &snapshot);
            return NULL;
        }

        func_pair_t pair = result.value;
        ir_type_t *result_type = module->funcs.funcs[pair.ir_func_id].return_type;

        if(out_type != NULL){
            *out_type = ast_type_clone(&ast->funcs[pair.ast_func_id].return_type);
        }

        return build_call(builder, pair.ir_func_id, result_type, arguments, 2);
    }

    return NULL;
}

ir_value_t *handle_math_management_allow_other_direction(ir_builder_t *builder, ir_math_operands_t *ops, source_t from_source, ast_type_t *out_type, weak_cstr_t overload_name){
    // Try LHS op RHS
    ir_value_t *attempt = handle_math_management(builder, ops, from_source, out_type, overload_name);
    if(attempt) return attempt;

    // Try RHS op LHS
    ir_math_operands_t flipped_ops = ir_math_operands_flipped(ops);
    return handle_math_management(builder, &flipped_ops, from_source, out_type, overload_name);
}

ir_value_t *handle_access_management(ir_builder_t *builder, ir_value_t *value, ir_value_t *index_value,
        ast_type_t *array_type, ast_type_t *index_type, ast_type_t *out_ptr_to_element_type){
    
    // NOTE: Attempts to use [] operator on a value that doesn't have a built-in version
    
    // We can only perform special access management if we're working with a pointer to a mutable struct-like
    if(
        value->type->kind != TYPE_KIND_POINTER ||
        ((ir_type_t*) value->type->extra)->kind != TYPE_KIND_STRUCTURE ||
        !ast_type_is_base_like(array_type)
    ){
        return NULL;
    }

    ir_pool_snapshot_t snapshot = ir_pool_snapshot_capture(builder->pool);

    optional_func_pair_t result;

    // Allocate argument list for upcoming __access__ method call
    ir_value_t **arguments = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * 2);
    arguments[0] = value;
    arguments[1] = index_value;

    // Allocate argument AST type list for upcoming __access__ method call
    ast_type_t argument_ast_types[2] = {
        ast_type_pointer_to(ast_type_clone(array_type)),
        *index_type,
    };

    weak_cstr_t struct_name = ast_type_struct_name(array_type);
    errorcode_t search_error = ir_gen_find_method_conforming_without_defaults(builder, struct_name, "__access__", arguments, argument_ast_types, 2, NULL, NULL_SOURCE, &result);

    ast_t *ast = &builder->object->ast;
    trait_t *arg_type_traits = ast->funcs[result.value.ast_func_id].arg_type_traits;

    bool error = search_error || !result.has || handle_pass_management(builder, arguments, ast->funcs[result.value.ast_func_id].arg_types, arg_type_traits, 2);
    ast_type_free(&argument_ast_types[0]);
    
    if(error){
        ir_pool_snapshot_restore(builder->pool, &snapshot);
        return NULL;
    }

    func_pair_t pair = result.value;
    ir_type_t *result_ir_type = builder->object->ir_module.funcs.funcs[pair.ir_func_id].return_type;

    if(out_ptr_to_element_type != NULL){
        *out_ptr_to_element_type = ast_type_clone(&ast->funcs[pair.ast_func_id].return_type);
    }

    return build_call(builder, pair.ir_func_id, result_ir_type, arguments, 2);
}

errorcode_t instantiate_poly_func(compiler_t *compiler, object_t *object, source_t instantiation_source, func_id_t ast_poly_func_id, ast_type_t *types,
        length_t types_list_length, ast_poly_catalog_t *catalog, ir_func_endpoint_t *out_endpoint){

    ast_func_t *poly_func = &object->ast.funcs[ast_poly_func_id];
    length_t required_arity = poly_func->arity;

    // Require compatible arity
    if(required_arity < types_list_length && !(poly_func->traits & (AST_FUNC_VARARG | AST_FUNC_VARIADIC))){
        return FAILURE;
    }
    
    // Require not disallowed
    if(poly_func->traits & AST_FUNC_DISALLOW) return FAILURE;

    // Determine whether we are missing arguments
    bool requires_use_of_defaults = required_arity > types_list_length;

    if(requires_use_of_defaults){
        // Check to make sure that we have the necessary default values available to later
        // fill in the missing arguments
        
        ast_expr_t **arg_defaults = poly_func->arg_defaults;

        // No default arguments are available to use to attempt to meet the arity requirement
        if(arg_defaults == NULL) return FAILURE;

        for(length_t i = types_list_length; i != required_arity; i++){
            // We are missing a necessary default argument value
            if(arg_defaults[i] == NULL) return FAILURE;
        }

        // Otherwise, we met the arity requirement.
        // We will then only process the argument values we already have
        // and leave processing and conforming the default arguments to higher level functions
    }
    
    ast_t *ast = &object->ast;
    func_id_t ast_func_id = ast_new_func(ast);

    // Revalidate poly_func
    poly_func = &object->ast.funcs[ast_poly_func_id];

    ast_func_t *func = &ast->funcs[ast_func_id];
    bool is_entry = streq(poly_func->name, compiler->entry_point);

    maybe_null_strong_cstr_t export_name = poly_func->export_as ? strclone(poly_func->export_as) : NULL;
    
    ast_func_prefixes_t prefixes = (ast_func_prefixes_t){
        .is_stdcall = poly_func->traits & AST_FUNC_STDCALL,
        .is_external = false,
        .is_implicit = poly_func->traits & AST_FUNC_IMPLICIT,
        .is_verbatim = !(poly_func->traits & AST_FUNC_AUTOGEN),
        .is_virtual = poly_func->traits & AST_FUNC_VIRTUAL,
        .is_override = poly_func->traits & AST_FUNC_OVERRIDE,
    };

    ast_func_create_template(func, &(ast_func_head_t){
        .name = strclone(poly_func->name),
        .source = poly_func->source,
        .is_foreign = false,
        .is_entry = is_entry,
        .prefixes = prefixes,
        .export_name = export_name,
    });

    func->arg_names = malloc(sizeof(weak_cstr_t) * poly_func->arity);
    func->arg_types = malloc(sizeof(ast_type_t) * poly_func->arity);
    func->arg_sources = malloc(sizeof(source_t) * poly_func->arity);
    func->arg_flows = malloc(sizeof(char) * poly_func->arity);
    func->arg_type_traits = malloc(sizeof(trait_t) * poly_func->arity);
    func->traits |= poly_func->traits & ~(AST_FUNC_MAIN | AST_FUNC_POLYMORPHIC);
    func->traits |= AST_FUNC_GENERATED | AST_FUNC_NO_SUGGEST;
    func->virtual_origin = poly_func->virtual_origin;

    if(poly_func->arg_defaults) {
        func->arg_defaults = malloc(sizeof(ast_expr_t *) * poly_func->arity);
        for(length_t i = 0; i != poly_func->arity; i++){
            func->arg_defaults[i] = poly_func->arg_defaults[i] ? ast_expr_clone(poly_func->arg_defaults[i]) : NULL;
        }
    } else {
        func->arg_defaults = NULL;
    }

    for(length_t i = 0; i != poly_func->arity; i++){
        ast_type_t *template_arg_type = &poly_func->arg_types[i];
        
        // Copy name of corresponding parameter
        func->arg_names[i] = strclone(poly_func->arg_names[i]);

        // Determine if we should use the type provided instead of the parameter type
        if(ast_type_has_polymorph(template_arg_type)){
            if(i >= types_list_length){
                source_t source = func->arg_defaults && func->arg_defaults[i] ? func->arg_defaults[i]->source : poly_func->source;
                compiler_panicf(compiler, source, "Cannot instantiate polymorphic function without specifying a non-default value for parameter");

                // Provide information of where the instantiation was triggered
                if(!SOURCE_IS_NULL(instantiation_source)){
                    compiler_panicf(compiler, instantiation_source, "Requires non-default arguments here");
                }

                return FAILURE;
            }

            // HACK: Allow for using unknown enum values for raw polymorphs (nothing else is allowed or supported)
            if(ast_type_is_unknown_enum(&types[i]) && ast_type_is_polymorph(template_arg_type)){
                ast_elem_polymorph_t *polymorph = (ast_elem_polymorph_t*) template_arg_type->elements[0];

                ast_poly_catalog_type_t *type = ast_poly_catalog_find_type(catalog, polymorph->name);
                assert(type != NULL);

                func->arg_types[i] = ast_type_clone(&type->binding);
            } else {
                func->arg_types[i] = ast_type_clone(&types[i]);
            }
        } else {
            func->arg_types[i] = ast_type_clone(template_arg_type);
        }
    }

    memcpy(func->arg_sources, poly_func->arg_sources, sizeof(source_t) * poly_func->arity);
    memcpy(func->arg_flows, poly_func->arg_flows, sizeof(char) * poly_func->arity);
    memcpy(func->arg_type_traits, poly_func->arg_type_traits, sizeof(trait_t) * poly_func->arity);

    func->arity = poly_func->arity;
    func->return_type = (ast_type_t){0};
    
    func->statements = ast_expr_list_clone(&poly_func->statements);

    if(ast_resolve_expr_list_polymorphs(compiler, object->ast.type_table, catalog, &func->statements)
    || ast_resolve_type_polymorphs(compiler, object->ast.type_table, catalog, &poly_func->return_type, &func->return_type)){
        goto failure;
    }

    ir_func_endpoint_t newest_endpoint;
    if(ir_gen_func_head(compiler, object, func, ast_func_id, &newest_endpoint)){
        goto failure;
    }

    if(func->traits & AST_FUNC_DISPATCHER){
        func_id_t ast_concrete_virtual_origin;

        if(instantiate_default_for_virtual_dispatcher(compiler, object, ast_func_id, instantiation_source, catalog, &ast_concrete_virtual_origin)){
            goto failure;
        }

        // Change virtual origin to be concrete version
        func = &ast->funcs[ast_func_id];
        func->virtual_origin = ast_concrete_virtual_origin;
    }

    if(out_endpoint) *out_endpoint = newest_endpoint;
    return SUCCESS;

failure:
    return FAILURE;
}

errorcode_t instantiate_default_for_virtual_dispatcher(
    compiler_t *compiler,
    object_t *object,
    func_id_t dispatcher_id,
    source_t instantiation_source,
    ast_poly_catalog_t *catalog,
    func_id_t *out_ast_concrete_virtual_origin
){
    // With the instantiation of a concrete dispatcher, instantiate the associated default implementation

    ast_t *ast = &object->ast;
    ast_func_t *dispatcher = &ast->funcs[dispatcher_id];

    assert(dispatcher->arity > 0);
    assert(ast_type_is_pointer_to_base_like(&dispatcher->arg_types[0]));

    func_id_t virtual_ast_id = dispatcher->virtual_origin;
    assert(virtual_ast_id != INVALID_FUNC_ID);

    ast_func_t *originating_virtual = &ast->funcs[virtual_ast_id];
    ast_type_t parent_type = ast_type_unwrapped_view(&originating_virtual->arg_types[0]);

    // Hook up link from concrete virtual to concrete dispatcher
    originating_virtual->virtual_dispatcher = dispatcher_id;

    // Since we already know that the child type extends the parent and that the supplied catalog can be used to resolve any polymorphism
    // to get there, we can just use the catalog the resolve any polymorphism in the parent type to get the concrete parent type.
    //
    // NOTE: Normally, we would have to run `ir_gen_does_extend()` first to ensure that it's the case and to get the right catalog,
    // but that is already guaranteed by the signature of all polymorphic dispatcher functions since they have a `$This extends OwnerType` clause
    ast_type_t concrete_parent_type;

    if(ast_resolve_type_polymorphs(compiler, object->ast.type_table, catalog, &parent_type, &concrete_parent_type)){
        goto failure;
    }

    // No function can be both a dispatcher and virtual origin
    assert(!(originating_virtual->traits & AST_FUNC_DISPATCHER));

    weak_cstr_t struct_name = ast_type_struct_name(&concrete_parent_type);
    weak_cstr_t method_name = originating_virtual->name;

    length_t arity = originating_virtual->arity;
    ast_type_t *arg_types = malloc(sizeof(ast_type_t) * arity);

    for(length_t i = 0; i != arity; i++){
        if(ast_resolve_type_polymorphs(compiler, object->ast.type_table, catalog, &originating_virtual->arg_types[i], &arg_types[i])){
            ast_types_free_fully(arg_types, arity);
            goto failure;
        }
    }

    // Create virtual function if necessary
    optional_func_pair_t result;
    errorcode_t errorcode = ir_gen_find_dispatchee(compiler, object, struct_name, method_name, arg_types, arity, instantiation_source, &result);

    ast_types_free_fully(arg_types, arity);

    if(errorcode || !result.has){
        compiler_panicf(compiler, instantiation_source, "Failed to get default implementation for virtual dispatcher");
        goto failure;
    }

    ast_func_t *default_impl = &object->ast.funcs[result.value.ast_func_id];

    if(!(default_impl->traits & AST_FUNC_VIRTUAL)){
        strong_cstr_t head_str = ast_func_head_str(default_impl);
        compiler_panicf(compiler, instantiation_source, "Overlapping contenders for default implementation of virtual function '%s'", head_str);
        free(head_str);
        goto failure;
    }

    *out_ast_concrete_virtual_origin = result.value.ast_func_id;

    ast_type_free(&concrete_parent_type);
    return SUCCESS;

failure:
    ast_type_free(&concrete_parent_type);
    return FAILURE;
}

errorcode_t attempt_autogen___defer__(compiler_t *compiler, object_t *object, ast_type_t *arg_types, length_t type_list_length, optional_func_pair_t *result){
    if(type_list_length != 1) return FAILURE;

    bool is_base_ptr = ast_type_is_base_ptr(&arg_types[0]);
    bool is_generic_base_ptr = ast_type_is_generic_base_ptr(&arg_types[0]);

    if(!is_base_ptr && !is_generic_base_ptr) return FAILURE;

    ast_type_t dereferenced_view = ast_type_dereferenced_view(&arg_types[0]);

    ast_t *ast = &object->ast;
    ir_gen_sf_cache_t *cache = &object->ir_module.sf_cache;
    ir_gen_sf_cache_entry_t *entry = ir_gen_sf_cache_locate_or_insert(cache, &dereferenced_view);

    if(ir_gen_sf_cache_read(entry->has_defer, entry->defer, result) == SUCCESS){
        return SUCCESS;
    }

    if(ast->funcs_length >= MAX_FUNC_ID){
        compiler_panic(compiler, arg_types[0].source, "Maximum number of AST functions reached\n");
        return FAILURE;
    }

    entry->has_defer = TROOLEAN_FALSE;

    ast_field_map_t field_map;

    // Cannot make if type is not a composite or if it's not simple
    if(is_base_ptr){
        weak_cstr_t struct_name = ((ast_elem_base_t*) arg_types[0].elements[1])->base;

        ast_composite_t *composite = ast_composite_find_exact(&object->ast, struct_name);

        // Require structure to exist
        if(composite == NULL) return FAILURE;

        // Don't handle children for complex composite types
        if(!ast_layout_is_simple_struct(&composite->layout)) return FAILURE;

        // Remember weak copy of field map
        field_map = composite->layout.field_map;
    } else if(is_generic_base_ptr){
        ast_elem_generic_base_t *generic_base = (ast_elem_generic_base_t*) arg_types[0].elements[1];
        weak_cstr_t struct_name = generic_base->name;
        ast_poly_composite_t *template = ast_poly_composite_find_exact(&object->ast, struct_name);

        // Require generic structure to exist
        if(template == NULL) return FAILURE;

        // Require generics count to match
        if(template->generics_length != generic_base->generics_length) return FAILURE;

        // Don't handle children for complex composite types
        if(!ast_layout_is_simple_struct(&template->layout)) return FAILURE;

        // Remember weak copy of field map
        field_map = template->layout.field_map;
    } else {
        internalerrorprintf("attempt_autogen___defer__() in state that should be unreachable\n");
        return FAILURE;
    }

    // See if any of the children will require a __defer__ call,
    // if not, return no function
    bool some_have_defer = false;

    for(length_t i = 0; i != field_map.arrows_length; i++){
        weak_cstr_t member = field_map.arrows[i].name;

        ir_field_info_t field_info;
        if(ir_gen_get_field_info(compiler, object, member, NULL_SOURCE, &dereferenced_view, &field_info)){
            return FAILURE;
        }

        optional_func_pair_t result;
        errorcode_t errorcode = ir_gen_find_defer_func(compiler, object, &field_info.ast_type, &result);
        ast_type_free(&field_info.ast_type);

        if(errorcode == ALT_FAILURE) return errorcode;

        if(errorcode == SUCCESS && result.has){
            some_have_defer = true;
            break;
        }
    }

    // If no children have __defer__ calls, return "none" function
    if(!some_have_defer){
        // Cache result
        entry->has_defer = TROOLEAN_FALSE;

        optional_func_pair_set(result, false, 0, 0);
        return SUCCESS;
    }

    // Create AST function
    func_id_t ast_func_id = ast_new_func(ast);
    ast_func_t *func = &ast->funcs[ast_func_id];

    ast_func_create_template(func, &(ast_func_head_t){
        .name = strclone("__defer__"),
        .source = NULL_SOURCE,
        .is_foreign = false,
        .is_entry = false,
        .prefixes = {0},
        .export_name = NULL,
    });

    func->traits |= AST_FUNC_AUTOGEN | AST_FUNC_GENERATED | AST_FUNC_NO_SUGGEST;

    func->arg_names = malloc(sizeof(weak_cstr_t));
    func->arg_types = malloc(sizeof(ast_type_t));
    func->arg_sources = malloc(sizeof(source_t));
    func->arg_flows = malloc(sizeof(char));
    func->arg_type_traits = malloc(sizeof(trait_t));

    func->arg_names[0] = strclone("this");
    func->arg_types[0] = ast_type_clone(&arg_types[0]);
    func->arg_sources[0] = NULL_SOURCE;
    func->arg_flows[0] = FLOW_IN;
    func->arg_type_traits[0] = AST_FUNC_ARG_TYPE_TRAIT_POD;
    func->arity = 1;

    memset(&func->statements, 0, sizeof(ast_expr_list_t));
    func->return_type = ast_type_make_base(strclone("void"));

    // Create IR function
    ir_func_endpoint_t newest_endpoint;
    if(ir_gen_func_head(compiler, object, func, ast_func_id, &newest_endpoint)){
        return FAILURE;
    }

    // Cache result
    entry->has_defer = TROOLEAN_TRUE;

    entry->defer = (func_pair_t){
        .ast_func_id = ast_func_id,
        .ir_func_id = newest_endpoint.ir_func_id,
    };

    // Return result
    optional_func_pair_set(result, true, ast_func_id, newest_endpoint.ir_func_id);
    return SUCCESS;
}

errorcode_t attempt_autogen___pass__(compiler_t *compiler, object_t *object, ast_type_t *arg_types, length_t type_list_length, optional_func_pair_t *result){
    if(type_list_length != 1) return FAILURE;

    bool is_base = ast_type_is_base(&arg_types[0]);
    bool is_generic_base = is_base ? false : ast_type_is_generic_base(&arg_types[0]);
    bool is_fixed_array = ast_type_is_fixed_array(&arg_types[0]);

    if(!(is_base || is_generic_base || is_fixed_array)){
        return FAILURE; // Require 'Type' or '<...> Type' for 'this' type
    }

    ir_gen_sf_cache_t *cache = &object->ir_module.sf_cache;
    ir_gen_sf_cache_entry_t *entry = ir_gen_sf_cache_locate_or_insert(cache, &arg_types[0]);

    if(ir_gen_sf_cache_read(entry->has_pass, entry->pass, result) == SUCCESS){
        return SUCCESS;
    }

    ast_t *ast = &object->ast;

    if(is_base){
        weak_cstr_t struct_name = ((ast_elem_base_t*) arg_types[0].elements[0])->base;

        ast_composite_t *composite = ast_composite_find_exact(&object->ast, struct_name);

        // Require structure to exist
        if(composite == NULL) return FAILURE;

        // Don't handle children for complex composite types
        if(!ast_layout_is_simple_struct(&composite->layout)) return SUCCESS;

    } else if(is_generic_base){
        ast_elem_generic_base_t *generic_base = (ast_elem_generic_base_t*) arg_types[0].elements[0];
        weak_cstr_t struct_name = generic_base->name;
        
        ast_poly_composite_t *template = ast_poly_composite_find_exact(&object->ast, struct_name);

        // Require generic structure to exist
        if(template == NULL) return FAILURE;

        // Require generics count to match
        if(template->generics_length != generic_base->generics_length) return FAILURE;

        // Don't handle children for complex composite types
        if(!ast_layout_is_simple_struct(&template->layout)) return SUCCESS;
    }

    if(ast->funcs_length >= MAX_FUNC_ID){
        compiler_panic(compiler, arg_types[0].source, "Maximum number of AST functions reached\n");
        return FAILURE;
    }

    func_id_t ast_func_id = ast_new_func(ast);
    ast_func_t *func = &ast->funcs[ast_func_id];
    
    ast_func_create_template(func, &(ast_func_head_t){
        .name = strclone("__pass__"),
        .source = NULL_SOURCE,
        .is_foreign = false,
        .is_entry = false,
        .prefixes = {0},
        .export_name = NULL,
    });

    func->traits |= AST_FUNC_AUTOGEN | AST_FUNC_GENERATED | AST_FUNC_NO_SUGGEST;

    func->arg_names = malloc(sizeof(weak_cstr_t) * 1);
    func->arg_types = malloc(sizeof(ast_type_t));
    func->arg_sources = malloc(sizeof(source_t));
    func->arg_flows = malloc(sizeof(char));
    func->arg_type_traits = malloc(sizeof(trait_t));

    func->arg_names[0] = strclone("passed");
    func->arg_types[0] = ast_type_clone(&arg_types[0]);
    func->arg_sources[0] = NULL_SOURCE;
    func->arg_flows[0] = FLOW_IN;
    func->arg_type_traits[0] = AST_FUNC_ARG_TYPE_TRAIT_POD;
    func->arity = 1;

    memset(&func->statements, 0, sizeof(ast_expr_list_t));

    func->return_type = ast_type_clone(&arg_types[0]);

    // Don't generate any statements because children will
    // be taken care of inside __defer__ function during IR generation
    // of the auto-generated function

    ir_func_endpoint_t newest_endpoint;
    if(ir_gen_func_head(compiler, object, func, ast_func_id, &newest_endpoint)){
        return FAILURE;
    }
    
    // Cache result
    entry->has_pass = TROOLEAN_TRUE;

    entry->pass = (func_pair_t){
        .ast_func_id = ast_func_id,
        .ir_func_id = newest_endpoint.ir_func_id,
    };

    optional_func_pair_set(result, true, ast_func_id, newest_endpoint.ir_func_id);
    return SUCCESS;
}

errorcode_t attempt_autogen___assign__(compiler_t *compiler, object_t *object, ast_type_t *arg_types, length_t type_list_length, optional_func_pair_t *result){
    if(type_list_length != 2) return FAILURE;

    bool subject_is_base_ptr = ast_type_is_base_ptr(&arg_types[0]);
    bool subject_is_generic_base_ptr = subject_is_base_ptr ? false : ast_type_is_generic_base_ptr(&arg_types[0]);

    if(!(subject_is_base_ptr || subject_is_generic_base_ptr)) return FAILURE;

    ast_type_t dereferenced_view = ast_type_dereferenced_view(&arg_types[0]);

    if(!ast_types_identical(&dereferenced_view, &arg_types[1])){
        return FAILURE;
    }

    ast_t *ast = &object->ast;
    ir_gen_sf_cache_t *cache = &object->ir_module.sf_cache;

    ir_gen_sf_cache_entry_t *entry = ir_gen_sf_cache_locate_or_insert(cache, &dereferenced_view);

    if(ir_gen_sf_cache_read(entry->has_assign, entry->assign, result) == SUCCESS){
        return SUCCESS;
    }

    if(ast->funcs_length >= MAX_FUNC_ID){
        compiler_panic(compiler, arg_types[0].source, "Maximum number of AST functions reached\n");
        return FAILURE;
    }

    entry->has_assign = TROOLEAN_FALSE;

    ast_field_map_t field_map;

    // Cannot make if type is not a composite or if it's not simple
    if(subject_is_base_ptr){
        weak_cstr_t struct_name = ((ast_elem_base_t*) arg_types[0].elements[1])->base;

        ast_composite_t *composite = ast_composite_find_exact(&object->ast, struct_name);

        // Require structure to exist
        if(composite == NULL){
            return FAILURE;
        }

        // Don't handle children for complex composite types
        if(!ast_layout_is_simple_struct(&composite->layout)){
            return FAILURE;
        }

        // Remember weak copy of field map
        field_map = composite->layout.field_map;
    } else if(subject_is_generic_base_ptr){
        ast_elem_generic_base_t *generic_base = (ast_elem_generic_base_t*) arg_types[0].elements[1];
        weak_cstr_t struct_name = generic_base->name;
        ast_poly_composite_t *template = ast_poly_composite_find_exact(&object->ast, struct_name);

        // Require generic structure to exist
        if(template == NULL){
            return FAILURE;
        }

        // Require generics count to match
        if(template->generics_length != generic_base->generics_length){
            return FAILURE;
        }

        // Don't handle children for complex composite types
        if(!ast_layout_is_simple_struct(&template->layout)){
            return FAILURE;
        }

        // Remember weak copy of field map
        field_map = template->layout.field_map;
    }

    // See if any of the children will require an __assign__ call,
    // if not, return no function
    bool some_have_assign = false;
    bool disallowed = false;

    for(length_t i = 0; i != field_map.arrows_length; i++){
        weak_cstr_t member = field_map.arrows[i].name;

        ir_field_info_t field_info;
        if(ir_gen_get_field_info(compiler, object, member, NULL_SOURCE, &dereferenced_view, &field_info)){
            return FAILURE;
        }

        optional_func_pair_t result;
        errorcode_t errorcode = ir_gen_find_assign_func(compiler, object, &field_info.ast_type, &result);
        
        ast_type_free(&field_info.ast_type);

        if(errorcode == ALT_FAILURE){
            return errorcode;
        }

        if(errorcode == SUCCESS && result.has){
            trait_t ast_func_traits = ast->funcs[result.value.ast_func_id].traits;

            some_have_assign = true;
            disallowed |= ast_func_traits & AST_FUNC_DISALLOW;
            break;
        }
    }

    // If no children have __assign__ calls, return "none" function
    if(!some_have_assign){
        // Cache result
        entry->has_assign = TROOLEAN_FALSE;

        optional_func_pair_set(result, false, 0, 0);
        return SUCCESS;
    }

    // Create AST function
    func_id_t ast_func_id = ast_new_func(ast);
    ast_func_t *func = &ast->funcs[ast_func_id];

    ast_func_create_template(func, &(ast_func_head_t){
        .name = strclone("__assign__"),
        .source = NULL_SOURCE,
        .is_foreign = false,
        .is_entry = false,
        .prefixes = {0},
        .export_name = NULL,
    });

    func->traits |= AST_FUNC_GENERATED | AST_FUNC_NO_SUGGEST;

    if(disallowed){
        func->traits |= AST_FUNC_DISALLOW;
    }

    func->arg_names = malloc(sizeof(weak_cstr_t) * 2);
    func->arg_types = malloc(sizeof(ast_type_t) * 2);
    func->arg_sources = malloc(sizeof(source_t) * 2);
    func->arg_flows = malloc(sizeof(char) * 2);
    func->arg_type_traits = malloc(sizeof(trait_t) * 2);

    func->arg_names[0] = strclone("this");
    func->arg_names[1] = strclone("$");
    func->arg_types[0] = ast_type_clone(&arg_types[0]);
    func->arg_types[1] = ast_type_clone(&arg_types[1]);
    func->arg_sources[0] = NULL_SOURCE;
    func->arg_sources[1] = NULL_SOURCE;
    func->arg_flows[0] = FLOW_IN;
    func->arg_flows[1] = FLOW_IN;
    func->arg_type_traits[0] = AST_FUNC_ARG_TYPE_TRAIT_POD;
    func->arg_type_traits[1] = AST_FUNC_ARG_TYPE_TRAIT_POD;
    func->arity = 2;
    func->statements = ast_expr_list_create(field_map.arrows_length);
    func->return_type = ast_type_make_base(strclone("void"));

    // Generate assignment statements
    for(length_t i = 0; i != field_map.arrows_length; i++){
        weak_cstr_t member = field_map.arrows[i].name;

        ast_expr_t *this_value = ast_expr_create_variable("this", NULL_SOURCE);
        ast_expr_t *other_value = ast_expr_create_variable("$", NULL_SOURCE);

        ast_expr_t *this_member = ast_expr_create_member(this_value, strclone(member), NULL_SOURCE);
        ast_expr_t *other_member = ast_expr_create_member(other_value, strclone(member), NULL_SOURCE);

        ast_expr_list_append(&func->statements, ast_expr_create_assignment(EXPR_ASSIGN, NULL_SOURCE, this_member, other_member, false));
    }

    // Create IR function
    ir_func_endpoint_t newest_endpoint;
    if(ir_gen_func_head(compiler, object, func, ast_func_id, &newest_endpoint)){
        return FAILURE;
    }

    // Cache result
    entry->has_assign = TROOLEAN_TRUE;
    
    entry->assign = (func_pair_t){
        .ast_func_id = ast_func_id,
        .ir_func_id = newest_endpoint.ir_func_id,
    };

    // Return result
    optional_func_pair_set(result, true, ast_func_id, newest_endpoint.ir_func_id);
    return SUCCESS;
}

// ---------------- is_allowed_builtin_auto_conversion ----------------
// Returns whether a builtin auto conversion is allowed
// (For integers / floats)
bool is_allowed_builtin_auto_conversion(compiler_t *compiler, object_t *object, const ast_type_t *a_type, const ast_type_t *b_type){
    if(!ast_type_is_base(a_type) || !ast_type_is_base(b_type)) return false;
    if(!typename_is_extended_builtin_type( ((ast_elem_base_t*) a_type->elements[0])->base )) return false;
    if(!typename_is_extended_builtin_type( ((ast_elem_base_t*) b_type->elements[0])->base )) return false;

    ir_pool_t *pool = &object->ir_module.pool;

    ir_pool_snapshot_t snapshot = ir_pool_snapshot_capture(pool);

    ir_type_t *a, *b;
    if(ir_gen_resolve_type(compiler, object, a_type, &a)) return false;

    if(ir_gen_resolve_type(compiler, object, b_type, &b)){
        ir_pool_snapshot_restore(pool, &snapshot);
        return false;
    }

    unsigned int a_kind = a->kind;
    unsigned int b_kind = b->kind;

    bool allowed = (global_type_kind_is_integer[a_kind] == global_type_kind_is_integer[b_kind] && global_type_kind_is_float[a_kind] == global_type_kind_is_float[b_kind]);
    ir_pool_snapshot_restore(pool, &snapshot);
    return allowed;
}

block_t *ir_builder_get_loop_label_info(ir_builder_t *builder, const char *label){
    block_stack_t *stack = &builder->block_stack;

    // Find loop label by name and return requested information
    for(length_t i = stack->length; i != 0; i--){
        if(streq(stack->blocks[i - 1].label, label)){
            return &stack->blocks[i - 1];
        }
    }

    return NULL;
}

errorcode_t ir_builder_get_noop_defer_func(ir_builder_t *builder, source_t source_on_error, func_id_t *out_ir_func_id){
    if(builder->has_noop_defer_function){
        *out_ir_func_id = builder->noop_defer_function;
        return SUCCESS;
    }

    func_id_t ir_func_id;
    if(ir_gen_func_template(builder->compiler, builder->object, "__noop_defer__", source_on_error, &ir_func_id)) return FAILURE;

    ir_module_t *module = &builder->object->ir_module;
    ir_func_t *module_func = &module->funcs.funcs[ir_func_id];
    module_func->argument_types = malloc(sizeof(ir_type_t) * 1);
    module_func->arity = 1;

    module_func->argument_types[0] = module->common.ir_ptr;
    module_func->return_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
    module_func->return_type->kind = TYPE_KIND_VOID;

    module_func->basicblocks = (ir_basicblocks_t){
        .blocks = malloc(sizeof(ir_basicblock_t)),
        .length = 1,
        .capacity = 1,
    };

    ir_basicblock_t *entry_block = &module_func->basicblocks.blocks[0];

    *entry_block = (ir_basicblock_t){
        .instructions = (ir_instrs_t){
            .instructions = malloc(sizeof(ir_instr_t)),
            .length = 1,
            .capacity = 1,
        },
        .traits = TRAIT_NONE,
    };

    ir_instr_ret_t *ret_void = (ir_instr_ret_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_ret_t));

    *ret_void = (ir_instr_ret_t){
        .id = INSTRUCTION_RET,
        .result_type = NULL,
        .value = NULL,
    };

    entry_block->instructions.instructions[0] = (ir_instr_t*) ret_void;

    builder->noop_defer_function = ir_func_id;
    builder->has_noop_defer_function = true;

    *out_ir_func_id = ir_func_id;
    return SUCCESS;
}

ir_value_t *ir_gen_actualize_unknown_enum(compiler_t *compiler, object_t *object, weak_cstr_t enum_name, weak_cstr_t kind_name, source_t source, ast_type_t *out_expr_type){
    ast_expr_t *enum_expr = ast_expr_create_enum_value(enum_name, kind_name, source);

    ir_value_t *result;
    if(ir_gen_expr_enum_value(compiler, object, (ast_expr_enum_value_t*) enum_expr, &result, out_expr_type)){
        ast_expr_free_fully(enum_expr);
        return NULL;
    }

    ast_expr_free_fully(enum_expr);
    return result;
}

instructions_snapshot_t instructions_snapshot_capture(ir_builder_t *builder){
    return (instructions_snapshot_t){
        .current_block_id = builder->current_block_id,
        .current_basicblock_instructions_length = builder->current_block->instructions.length,
        .basicblocks_length = builder->basicblocks.length,
    };
}

void instructions_snapshot_restore(ir_builder_t *builder, instructions_snapshot_t *snapshot){
    builder->current_block_id = snapshot->current_block_id;
    builder->current_block = &builder->basicblocks.blocks[builder->current_block_id];
    builder->current_block->instructions.length = snapshot->current_basicblock_instructions_length;
    builder->basicblocks.length = snapshot->basicblocks_length;
}
