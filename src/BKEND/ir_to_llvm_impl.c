
#include <assert.h>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm/Config/llvm-config.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "BKEND/ir_to_llvm.h"
#include "BRIDGE/bridge.h"
#include "DRVR/compiler.h"
#include "DRVR/object.h"
#include "IR/ir.h"
#include "IR/ir_type.h"
#include "IR/ir_value.h"
#include "IRGEN/ir_builder.h"
#include "UTIL/color.h"
#include "UTIL/datatypes.h"
#include "UTIL/ground.h"
#include "UTIL/util.h"
#include "llvm-c/Analysis.h" // IWYU pragma: keep
#include "llvm-c/TargetMachine.h"
#include "llvm-c/Types.h"

LLVMTypeRef ir_to_llvm_type(llvm_context_t *llvm, ir_type_t *ir_type){
    // Converts an ir type to an llvm type
    LLVMTypeRef type_ref_tmp;

    switch(ir_type->kind){
    case TYPE_KIND_POINTER:
        type_ref_tmp = ir_to_llvm_type(llvm, (ir_type_t*) ir_type->extra);
        if(type_ref_tmp == NULL) return NULL;
        return LLVMPointerType(type_ref_tmp, 0);
    case TYPE_KIND_S8:      return LLVMInt8Type();
    case TYPE_KIND_S16:     return LLVMInt16Type();
    case TYPE_KIND_S32:     return LLVMInt32Type();
    case TYPE_KIND_S64:     return llvm->i64_type;
    case TYPE_KIND_U8:      return LLVMInt8Type();
    case TYPE_KIND_U16:     return LLVMInt16Type();
    case TYPE_KIND_U32:     return LLVMInt32Type();
    case TYPE_KIND_U64:     return llvm->i64_type;
    case TYPE_KIND_HALF:    return LLVMHalfType();
    case TYPE_KIND_FLOAT:   return LLVMFloatType();
    case TYPE_KIND_DOUBLE:  return llvm->f64_type;
    case TYPE_KIND_BOOLEAN: return LLVMInt1Type();
    case TYPE_KIND_STRUCTURE: {
            // TODO: Should probably cache struct types so they don't have to be
            //           remade into LLVM types every time we use them.

            ir_type_extra_composite_t *composite = (ir_type_extra_composite_t*) ir_type->extra;
            LLVMTypeRef fields[composite->subtypes_length];

            for(length_t i = 0; i != composite->subtypes_length; i++){
                fields[i] = ir_to_llvm_type(llvm, composite->subtypes[i]);
                if(fields[i] == NULL) return NULL;
            }

            return LLVMStructType(fields, composite->subtypes_length, composite->traits & TYPE_KIND_COMPOSITE_PACKED);
        }
    case TYPE_KIND_UNION: {
            // TODO: Should probably cache union types so they don't have to be
            //           remade into LLVM types every time we use them.

            ir_type_extra_composite_t *composite = (ir_type_extra_composite_t*) ir_type->extra;
            LLVMTypeRef fields[composite->subtypes_length];

            length_t largest_size = 0;

            for(length_t i = 0; i != composite->subtypes_length; i++){
                fields[i] = ir_to_llvm_type(llvm, composite->subtypes[i]);
                if(fields[i] == NULL) return NULL;
                length_t type_size = LLVMABISizeOfType(llvm->data_layout, fields[i]);
                if(largest_size < type_size) largest_size = type_size;
            }

            // Force non-zero size (since LLVM doesn't like zero sized arrays)
            if(largest_size == 0) largest_size = 1;

            if(composite->traits & TYPE_KIND_COMPOSITE_PACKED){
                // Packed Unions
                return LLVMArrayType(LLVMInt8Type(), largest_size);
            } else {
                // Unpacked Unions

                // Do some black magic to get good alignment
                length_t chosen_element_size = largest_size >= 8 ? 8 : largest_size;
                LLVMTypeRef chosen_element_type = LLVMIntType(chosen_element_size * 8);
                length_t extra_one = largest_size % chosen_element_size != 0 ? 1 : 0;
                
                return LLVMArrayType(chosen_element_type, largest_size / chosen_element_size + extra_one);
            }
        }
        break;
    case TYPE_KIND_VOID: return LLVMVoidType();
    case TYPE_KIND_FUNCPTR: {
            ir_type_extra_function_t *function = (ir_type_extra_function_t*) ir_type->extra;
            LLVMTypeRef args[function->arity];

            for(length_t i = 0; i != function->arity; i++){
                args[i] = ir_to_llvm_type(llvm, function->arg_types[i]);
                if(args[i] == NULL) return NULL;
            }

            LLVMTypeRef type_ref_tmp = LLVMFunctionType(ir_to_llvm_type(llvm, function->return_type),
                args, function->arity, function->traits & TYPE_KIND_FUNC_VARARG);
            type_ref_tmp = LLVMPointerType(type_ref_tmp, 0);

            return type_ref_tmp;
        }
        break;
    case TYPE_KIND_FIXED_ARRAY:{
            ir_type_extra_fixed_array_t *fixed_array = (ir_type_extra_fixed_array_t*) ir_type->extra;
            type_ref_tmp = ir_to_llvm_type(llvm, fixed_array->subtype);
            if(type_ref_tmp == NULL) return NULL;
            return LLVMArrayType(type_ref_tmp, fixed_array->length);
        }
        break;
    default: return NULL; // No suitable llvm type
    }
}

LLVMValueRef ir_to_llvm_value(llvm_context_t *llvm, ir_value_t *value){
    // Retrieves a literal or previously computed value

    if(value == NULL){
        die("ir_to_llvm_value() - Received NULL pointer\n");
    }

    switch(value->value_type){
    case VALUE_TYPE_LITERAL: {
            switch(value->type->kind){
            case TYPE_KIND_S8: return LLVMConstInt(LLVMInt8Type(), *((adept_byte*) value->extra), true);
            case TYPE_KIND_U8: return LLVMConstInt(LLVMInt8Type(), *((adept_ubyte*) value->extra), false);
            case TYPE_KIND_S16: return LLVMConstInt(LLVMInt16Type(), *((adept_short*) value->extra), true);
            case TYPE_KIND_U16: return LLVMConstInt(LLVMInt16Type(), *((adept_ushort*) value->extra), false);
            case TYPE_KIND_S32: return LLVMConstInt(LLVMInt32Type(), *((adept_int*) value->extra), true);
            case TYPE_KIND_U32: return LLVMConstInt(LLVMInt32Type(), *((adept_uint*) value->extra), false);
            case TYPE_KIND_S64: return LLVMConstInt(llvm->i64_type, *((adept_long *)value->extra), true);
            case TYPE_KIND_U64: return LLVMConstInt(llvm->i64_type, *((adept_ulong *)value->extra), false);
            case TYPE_KIND_FLOAT: return LLVMConstReal(LLVMFloatType(), *((adept_float*) value->extra));
            case TYPE_KIND_DOUBLE: return LLVMConstReal(llvm->f64_type, *((adept_double*) value->extra));
            case TYPE_KIND_BOOLEAN: return LLVMConstInt(LLVMInt1Type(), *((adept_bool*) value->extra), false);
            default:
                die("ir_to_llvm_value() - Unrecognized type kind for literal in ir_to_llvm_value\n");
            }
        }
    case VALUE_TYPE_RESULT: {
            ir_value_result_t *extra = (ir_value_result_t*) value->extra;
            return llvm->catalog->blocks[extra->block_id].value_references[extra->instruction_id];
        }
    case VALUE_TYPE_NULLPTR:
        return LLVMConstNull(LLVMPointerType(LLVMInt8Type(), 0));
    case VALUE_TYPE_NULLPTR_OF_TYPE:
        return LLVMConstNull(ir_to_llvm_type(llvm, value->type));
    case VALUE_TYPE_ARRAY_LITERAL: {
            ir_value_array_literal_t *array_literal = value->extra;

            // Assume that value->type is a pointer to array element type
            LLVMTypeRef type = ir_to_llvm_type(llvm, (ir_type_t*) value->type->extra);
            
            LLVMValueRef values[array_literal->length];

            for(length_t i = 0; i != array_literal->length; i++){
                // Assumes ir_value_t values are constants (should have been checked earlier)
                values[i] = ir_to_llvm_value(llvm, array_literal->values[i]);
            }

            LLVMValueRef static_array = LLVMConstArray(type, values, array_literal->length);
            LLVMValueRef global_data = LLVMAddGlobal(llvm->module, LLVMArrayType(type, array_literal->length), "A");
            LLVMSetLinkage(global_data, LLVMInternalLinkage);
            LLVMSetGlobalConstant(global_data, true);
            LLVMSetInitializer(global_data, static_array);

            LLVMValueRef indices[2];
            indices[0] = LLVMConstInt(LLVMInt32Type(), 0, true);
            indices[1] = LLVMConstInt(LLVMInt32Type(), 0, true);

            return LLVMConstGEP(global_data, indices, 2);
        }
    case VALUE_TYPE_STRUCT_LITERAL: {
            ir_value_struct_literal_t *struct_literal = value->extra;

            // Assume that value->type is a pointer to a struct
            LLVMTypeRef type = ir_to_llvm_type(llvm, value->type);
            
            LLVMValueRef values[struct_literal->length];

            for(length_t i = 0; i != struct_literal->length; i++){
                // Assumes ir_value_t values are constants (should have been checked earlier)
                values[i] = ir_to_llvm_value(llvm, struct_literal->values[i]);
            }
            
            return LLVMConstNamedStruct(type, values, struct_literal->length);
        }
    case VALUE_TYPE_ANON_GLOBAL: case VALUE_TYPE_CONST_ANON_GLOBAL: {
            ir_value_anon_global_t *extra = (ir_value_anon_global_t*) value->extra;
            return llvm->anon_global_variables[extra->anon_global_id];
        }
    case VALUE_TYPE_CSTR_OF_LEN: {
            ir_value_cstr_of_len_t *cstr_of_len = value->extra;
            LLVMValueRef global_data = llvm_string_table_find(&llvm->string_table, cstr_of_len->array, cstr_of_len->size);

            if(global_data == NULL){
                // DANGEROUS: TODO: Remove this!!!
                static int i = 0;
                char name_buffer[256];
                sprintf(name_buffer, "S%X", i++);
                global_data = LLVMAddGlobal(llvm->module, LLVMArrayType(LLVMInt8Type(), cstr_of_len->size), name_buffer);
                LLVMSetLinkage(global_data, LLVMInternalLinkage);
                LLVMSetGlobalConstant(global_data, true);
                LLVMSetInitializer(global_data, LLVMConstString(cstr_of_len->array, cstr_of_len->size, true));

                llvm_string_table_add(&llvm->string_table, cstr_of_len->array, cstr_of_len->size, global_data);
            }
            
            LLVMValueRef indices[2];
            indices[0] = LLVMConstInt(LLVMInt32Type(), 0, true);
            indices[1] = LLVMConstInt(LLVMInt32Type(), 0, true);
            return LLVMConstGEP(global_data, indices, 2);
        }
    case VALUE_TYPE_CONST_BITCAST:
            return LLVMConstBitCast(ir_to_llvm_value(llvm, value->extra), ir_to_llvm_type(llvm, value->type));
    case VALUE_TYPE_CONST_ZEXT:
            return LLVMConstZExt(ir_to_llvm_value(llvm, value->extra), ir_to_llvm_type(llvm, value->type));
    case VALUE_TYPE_CONST_SEXT:
            return LLVMConstSExt(ir_to_llvm_value(llvm, value->extra), ir_to_llvm_type(llvm, value->type));
    case VALUE_TYPE_CONST_FEXT:
            return LLVMConstFPExt(ir_to_llvm_value(llvm, value->extra), ir_to_llvm_type(llvm, value->type));
    case VALUE_TYPE_CONST_TRUNC:
            return LLVMConstTrunc(ir_to_llvm_value(llvm, value->extra), ir_to_llvm_type(llvm, value->type));
    case VALUE_TYPE_CONST_FTRUNC:
            return LLVMConstFPTrunc(ir_to_llvm_value(llvm, value->extra), ir_to_llvm_type(llvm, value->type));
    case VALUE_TYPE_CONST_INTTOPTR:
            return LLVMConstIntToPtr(ir_to_llvm_value(llvm, value->extra), ir_to_llvm_type(llvm, value->type));
    case VALUE_TYPE_CONST_PTRTOINT:
            return LLVMConstPtrToInt(ir_to_llvm_value(llvm, value->extra), ir_to_llvm_type(llvm, value->type));
    case VALUE_TYPE_CONST_FPTOUI:
            return LLVMConstFPToUI(ir_to_llvm_value(llvm, value->extra), ir_to_llvm_type(llvm, value->type));
    case VALUE_TYPE_CONST_FPTOSI:
            return LLVMConstFPToSI(ir_to_llvm_value(llvm, value->extra), ir_to_llvm_type(llvm, value->type));
    case VALUE_TYPE_CONST_UITOFP:
            return LLVMConstUIToFP(ir_to_llvm_value(llvm, value->extra), ir_to_llvm_type(llvm, value->type));
    case VALUE_TYPE_CONST_SITOFP:
            return LLVMConstSIToFP(ir_to_llvm_value(llvm, value->extra), ir_to_llvm_type(llvm, value->type));
    case VALUE_TYPE_CONST_REINTERPRET:
            return ir_to_llvm_value(llvm, value->extra);
    case VALUE_TYPE_STRUCT_CONSTRUCTION: {
            ir_value_struct_construction_t *construction = (ir_value_struct_construction_t*) value->extra;

            LLVMValueRef constructed = LLVMGetUndef(ir_to_llvm_type(llvm, value->type));

            for(length_t i = 0; i != construction->length; i++){
                constructed = LLVMBuildInsertValue(llvm->builder, constructed, ir_to_llvm_value(llvm, construction->values[i]), i, "");
            }

            return constructed;
        }
    case VALUE_TYPE_OFFSETOF: {
            ir_value_offsetof_t *offsetof = (ir_value_offsetof_t*) value->extra;
            unsigned long long offset = LLVMOffsetOfElement(llvm->data_layout, ir_to_llvm_type(llvm, offsetof->type), offsetof->index);
            return LLVMConstInt(llvm->i64_type, offset, false);
        }
    case VALUE_TYPE_CONST_SIZEOF: {
            ir_value_const_sizeof_t *const_sizeof = (ir_value_const_sizeof_t*) value->extra;
            length_t type_size = const_sizeof->type->kind == TYPE_KIND_VOID ? 0 : LLVMABISizeOfType(llvm->data_layout, ir_to_llvm_type(llvm, const_sizeof->type));
            return LLVMConstInt(llvm->i64_type, type_size, false);
        }
    case VALUE_TYPE_CONST_ALIGNOF: {
            ir_value_const_alignof_t *const_alignof = (ir_value_const_alignof_t*) value->extra;
            length_t type_size = const_alignof->type->kind == TYPE_KIND_VOID ? 0 : LLVMABIAlignmentOfType(llvm->data_layout, ir_to_llvm_type(llvm, const_alignof->type));
            return LLVMConstInt(llvm->i64_type, type_size, false);
        }
    case VALUE_TYPE_CONST_ADD: {
            ir_value_const_math_t *const_add = (ir_value_const_math_t*) value->extra;
            return LLVMConstAdd(ir_to_llvm_value(llvm, const_add->a), ir_to_llvm_value(llvm, const_add->b));
        }
    default:
        die("ir_to_llvm_value() - Unrecognized value type %d\n", (int) value->value_type);
    }

    return NULL;
}

errorcode_t ir_to_llvm_functions(llvm_context_t *llvm, object_t *object){
    // Generates llvm function skeletons from ir function data

    LLVMModuleRef llvm_module = llvm->module;
    ir_func_t *module_funcs = object->ir_module.funcs.funcs;
    length_t module_funcs_length = object->ir_module.funcs.length;
    LLVMValueRef *func_skeletons = llvm->func_skeletons;

    LLVMAttributeRef nounwind = LLVMCreateEnumAttribute(LLVMGetGlobalContext(), LLVMGetEnumAttributeKindForName("nounwind", 8), 0);

    for(length_t ir_func_id = 0; ir_func_id != module_funcs_length; ir_func_id++){
        ir_func_t *ir_func = &module_funcs[ir_func_id];
        LLVMTypeRef parameters[ir_func->arity];

        for(length_t a = 0; a != ir_func->arity; a++){
            parameters[a] = ir_to_llvm_type(llvm, ir_func->argument_types[a]);
            if(parameters[a] == NULL) return FAILURE;
        }

        LLVMTypeRef return_type = ir_to_llvm_type(llvm, ir_func->return_type);
        LLVMTypeRef llvm_func_type = LLVMFunctionType(return_type, parameters, ir_func->arity, ir_func->traits & IR_FUNC_VARARG);

        LLVMValueRef *skeleton = &func_skeletons[ir_func_id];

        if(ir_func->traits & IR_FUNC_FOREIGN){
            *skeleton = LLVMAddFunction(llvm_module, compiler_unnamespaced_name(ir_func->name), llvm_func_type);
        } else if(ir_func->traits & IR_FUNC_MAIN){
            *skeleton = LLVMAddFunction(llvm_module, "main", llvm_func_type);
        } else if(ir_func->export_as){
            *skeleton = LLVMAddFunction(llvm_module, ir_func->export_as, llvm_func_type);
        } else {
            char adept_implementation_name[256];
            ir_implementation(ir_func_id, 'a', adept_implementation_name);

            *skeleton = LLVMAddFunction(llvm_module, adept_implementation_name, llvm_func_type);
            LLVMSetLinkage(*skeleton, LLVMPrivateLinkage);
        }

        LLVMCallConv call_conv = ir_func->traits & IR_FUNC_STDCALL ? LLVMX86StdcallCallConv : LLVMCCallConv;
        LLVMSetFunctionCallConv(*skeleton, call_conv);
        
        // Add nounwind to everything that isn't foreign
        if(!(ir_func->traits & IR_FUNC_FOREIGN))
            LLVMAddAttributeAtIndex(*skeleton, LLVMAttributeFunctionIndex, nounwind);
    }

    // Generate function to handle deinitialization of static variables
    if(ir_to_llvm_generate_deinit_svars_function_head(llvm)) return FAILURE;

    return SUCCESS;
}

errorcode_t ir_to_llvm_function_bodies(llvm_context_t *llvm, object_t *object){
    // Generates llvm function bodies from ir function data
    // NOTE: Expects function skeletons to already be present

    ir_func_t *module_funcs = object->ir_module.funcs.funcs;
    length_t module_funcs_length = object->ir_module.funcs.length;
    LLVMValueRef *func_skeletons = llvm->func_skeletons;

    llvm->relocation_list = (llvm_phi2_relocation_list_t){0};

    for(length_t f = 0; f != module_funcs_length; f++){
        LLVMBuilderRef builder = LLVMCreateBuilder();
        ir_basicblocks_t basicblocks = module_funcs[f].basicblocks;

        value_catalog_t catalog;
        value_catalog_prepare(&catalog, basicblocks);

        varstack_t stack;
        stack.values = malloc(sizeof(LLVMValueRef) * module_funcs[f].variable_count);
        stack.types = malloc(sizeof(LLVMTypeRef) * module_funcs[f].variable_count);
        stack.length = module_funcs[f].variable_count;

        llvm->builder = builder;
        llvm->catalog = &catalog;
        llvm->stack = &stack;

        LLVMBasicBlockRef *llvm_blocks = malloc(sizeof(LLVMBasicBlockRef) * basicblocks.length);

        // If the true exit point of a block changed, its real value will be in here
        // (Used for PHI instructions to have the proper end point)
        LLVMBasicBlockRef *llvm_exit_blocks = malloc(sizeof(LLVMBasicBlockRef) * basicblocks.length);

        // Information about PHI instructions that need to have their incoming blocks delayed
        llvm->relocation_list.length = 0;

        // Determine whether this function is the entry point
        bool is_entry_function = module_funcs[f].traits & IR_FUNC_MAIN && llvm->static_variable_info.init_routine == NULL;

        // Inject true entry before faux program entry
        if(is_entry_function) llvm->static_variable_info.init_routine = LLVMAppendBasicBlock(func_skeletons[f], "");

        // Create basicblocks
        for(length_t b = 0; b != basicblocks.length; b++){
            llvm_blocks[b] = LLVMAppendBasicBlock(func_skeletons[f], "");
            llvm_exit_blocks[b] = llvm_blocks[b];
        }

        // Remember faux basicblock entry of main function
        if(is_entry_function) llvm->static_variable_info.init_post = llvm_blocks[0];

        // Drop references to any old PHIs
        llvm->null_check.line_phi = NULL;
        llvm->null_check.column_phi = NULL;

        errorcode_t errorcode = ir_to_llvm_basicblocks(llvm, basicblocks,
            func_skeletons[f], &module_funcs[f], llvm_blocks, llvm_exit_blocks, f);

        value_catalog_free(&catalog);
        free(stack.values);
        free(stack.types);
        free(llvm_blocks);
        free(llvm_exit_blocks);
        LLVMDisposeBuilder(builder);

        if(errorcode) return errorcode;
    }

    return SUCCESS;
}

errorcode_t ir_to_llvm_basicblocks(llvm_context_t *llvm, ir_basicblocks_t basicblocks, LLVMValueRef func_skeleton,
        ir_func_t *module_func, LLVMBasicBlockRef *llvm_blocks, LLVMBasicBlockRef *llvm_exit_blocks, length_t f){
    
    LLVMBuilderRef builder = llvm->builder;
    varstack_t *stack = llvm->stack;

    if(llvm->compiler->checks & COMPILER_NULL_CHECKS && basicblocks.length != 0){
        build_llvm_null_check_on_failure_block(llvm, func_skeleton, module_func);
    }
    
    for(length_t b = 0; b != basicblocks.length; b++){
        LLVMPositionBuilderAtEnd(builder, llvm_blocks[b]);
        ir_basicblock_t *basicblock = &basicblocks.blocks[b];

        // Allocate stack variables
        if(b == 0 && ir_to_llvm_allocate_stack_variables(llvm, stack, func_skeleton, module_func)) return FAILURE;

        // Generate instructions
        if(ir_to_llvm_instructions(llvm, basicblock->instructions, b, f, llvm_blocks, llvm_exit_blocks)){
            return FAILURE;
        }
    }

    // Relocate incoming references for basicblocks
    for(length_t i = 0; i != llvm->relocation_list.length; i++){
        llvm_phi2_relocation_t *relocation = &llvm->relocation_list.unrelocated[i];

        // Now that we have information about the correct exit blocks, we can fill in
        // the incoming blocks
        LLVMAddIncoming(relocation->phi, &relocation->a, &llvm_exit_blocks[relocation->basicblock_a], 1);
        LLVMAddIncoming(relocation->phi, &relocation->b, &llvm_exit_blocks[relocation->basicblock_b], 1);
    }

    // Remove basicblock and PHI nodes for null check failure pseudo-function if not used
    if(llvm->compiler->checks & COMPILER_NULL_CHECKS) {
        // NOTE: Assumes (LLVMCountIncoming(llvm->line_phi) == LLVMCountIncoming(llvm->column_phi))
        if(llvm->null_check.line_phi && LLVMCountIncoming(llvm->null_check.line_phi) == 0 && llvm->null_check.on_fail_block){
            LLVMDeleteBasicBlock(llvm->null_check.on_fail_block);
        }
    }

    return SUCCESS;
}

errorcode_t ir_to_llvm_allocate_stack_variables(llvm_context_t *llvm, varstack_t *stack, LLVMValueRef func_skeleton, ir_func_t *module_func){
    LLVMBuilderRef builder = llvm->builder;

    for(length_t i = 0; i != stack->length; i++){
        bridge_var_t *var = bridge_scope_find_var_by_id(module_func->scope, i);

        if(var == NULL){
            die("ir_to_llvm_allocate_stack_variables() - Variable with ID %d could not be found\n", (int) i);
        }

        LLVMTypeRef alloca_type = ir_to_llvm_type(llvm, var->ir_type);

        if(alloca_type == NULL){
            return FAILURE;
        }

        if(var->traits & BRIDGE_VAR_STATIC){
            stack->values[i] = llvm->static_variables.variables[var->static_id].global;
        } else {
            stack->values[i] = LLVMBuildAlloca(builder, alloca_type, "");
        }

        stack->types[i] = alloca_type;

        if(i < module_func->arity){
            // Function argument that needs passed argument value
            LLVMBuildStore(builder, LLVMGetParam(func_skeleton, i), stack->values[i]);
        }
    }

    return SUCCESS;
}

void build_llvm_null_check_on_failure_block(llvm_context_t *llvm, LLVMValueRef func_skeleton, ir_func_t *module_func){
    LLVMBuilderRef builder = llvm->builder;

    // Line number and column number and created via a PHI node
    // when we call pseudo-function to handle null check failures
    // Create pseudo-function
    llvm->null_check.on_fail_block = LLVMAppendBasicBlock(func_skeleton, "");
    LLVMPositionBuilderAtEnd(builder, llvm->null_check.on_fail_block);

    // Establish dependencies and define them if necessary
    LLVMValueRef printf_fn = LLVMGetNamedFunction(llvm->module, "printf");
    LLVMValueRef exit_fn = LLVMGetNamedFunction(llvm->module, "exit");

    if(exit_fn == NULL){
        LLVMTypeRef int32 = LLVMInt32Type();
        LLVMTypeRef exit_fn_type = LLVMFunctionType(int32, &int32, 1, false);
        exit_fn = LLVMAddFunction(llvm->module, "exit", exit_fn_type);
    }

    if(printf_fn == NULL){
        LLVMTypeRef int32 = LLVMInt32Type();
        LLVMTypeRef charptr = LLVMPointerType(LLVMInt8Type(), 0);
        LLVMTypeRef printf_fn_type = LLVMFunctionType(int32, &charptr, 1, true);
        printf_fn = LLVMAddFunction(llvm->module, "printf", printf_fn_type);
    }

    LLVMValueRef global_data;

    // Create template error message
    if(llvm->null_check.failure_message_bytes == NULL){
        const char *error_msg = "===== RUNTIME ERROR: NULL POINTER DEREFERENCE, MEMBER-ACCESS, OR ELEMENT-ACCESS! =====\nIn file:\t%s\nIn function:\t%s\nLine:\t%d\nColumn:\t%d\n";
        length_t error_msg_length = strlen(error_msg) + 1;
        global_data = LLVMAddGlobal(llvm->module, LLVMArrayType(LLVMInt8Type(), error_msg_length), ".str");
        LLVMSetLinkage(global_data, LLVMInternalLinkage);
        LLVMSetGlobalConstant(global_data, true);
        LLVMSetInitializer(global_data, LLVMConstString(error_msg, error_msg_length, true));
        llvm->null_check.failure_message_bytes = global_data;
    }

    LLVMValueRef indices[2];
    indices[0] = LLVMConstInt(LLVMInt32Type(), 0, true);
    indices[1] = LLVMConstInt(LLVMInt32Type(), 0, true);
    LLVMValueRef arg = LLVMConstGEP(llvm->null_check.failure_message_bytes, indices, 2);

    // Decide on filename to use for error message
    const char *filename = module_func->maybe_filename;
    if(filename == NULL) filename = "<unknown file>";

    // Decide on function definition string to use for error function
    const char *func_name = module_func->maybe_definition_string;
    if(func_name == NULL) func_name = module_func->name;

    // Define filename string
    length_t filename_len = strlen(filename) + 1;
    global_data = LLVMAddGlobal(llvm->module, LLVMArrayType(LLVMInt8Type(), filename_len), ".str");
    LLVMSetLinkage(global_data, LLVMInternalLinkage);
    LLVMSetGlobalConstant(global_data, true);
    LLVMSetInitializer(global_data, LLVMConstString(filename, filename_len, true));
    indices[0] = LLVMConstInt(LLVMInt32Type(), 0, true);
    indices[1] = LLVMConstInt(LLVMInt32Type(), 0, true);
    LLVMValueRef filename_str = LLVMConstGEP(global_data, indices, 2);

    // Define function definition string
    length_t func_name_len = strlen(func_name) + 1;
    global_data = LLVMAddGlobal(llvm->module, LLVMArrayType(LLVMInt8Type(), func_name_len), ".str");
    LLVMSetLinkage(global_data, LLVMInternalLinkage);
    LLVMSetGlobalConstant(global_data, true);
    LLVMSetInitializer(global_data, LLVMConstString(func_name, func_name_len, true));
    indices[0] = LLVMConstInt(LLVMInt32Type(), 0, true);
    indices[1] = LLVMConstInt(LLVMInt32Type(), 0, true);
    LLVMValueRef func_name_str = LLVMConstGEP(global_data, indices, 2);

    llvm->null_check.line_phi = LLVMBuildPhi(llvm->builder, LLVMInt32Type(), "");
    llvm->null_check.column_phi = LLVMBuildPhi(llvm->builder, LLVMInt32Type(), "");

    // Create argument list
    LLVMValueRef args[] = {arg, filename_str, func_name_str, llvm->null_check.line_phi, llvm->null_check.column_phi};

    // Print the error message
    LLVMBuildCall(builder, printf_fn, args, 5, "");

    // Exit the program
    arg = LLVMConstInt(LLVMInt32Type(), 1, true);
    LLVMBuildCall(builder, exit_fn, &arg, 1, "");
    LLVMBuildUnreachable(builder);
}

errorcode_t ir_to_llvm_instructions(llvm_context_t *llvm, ir_instrs_t instructions, length_t basicblock_id,
        length_t f, LLVMBasicBlockRef *llvm_blocks, LLVMBasicBlockRef *llvm_exit_blocks){
    length_t b = basicblock_id;
    LLVMBuilderRef builder = llvm->builder;
    value_catalog_t *catalog = llvm->catalog;
    LLVMValueRef llvm_result;

    for(length_t i = 0; i != instructions.length; i++){
        ir_instr_t *instr = instructions.instructions[i];

        switch(instr->id){
        case INSTRUCTION_RET:
            LLVMBuildRet(builder, ((ir_instr_ret_t*) instr)->value == NULL ? NULL : ir_to_llvm_value(llvm, ((ir_instr_ret_t*) instr)->value));
            break;
        case INSTRUCTION_ADD:
            // Do excess instructions for adding pointers to get llvm to shut up
            if(((ir_instr_math_t*) instr)->a->type->kind == TYPE_KIND_POINTER){
                LLVMValueRef val_a = ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a);
                LLVMValueRef val_b = ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b);
                val_a = LLVMBuildPtrToInt(builder, val_a, llvm->i64_type, "");
                val_b = LLVMBuildPtrToInt(builder, val_b, llvm->i64_type, "");
                llvm_result = LLVMBuildAdd(builder, val_a, val_b, "");
                llvm_result = LLVMBuildIntToPtr(builder, llvm_result, ir_to_llvm_type(llvm, ((ir_instr_math_t*) instr)->a->type), "");
                catalog->blocks[b].value_references[i] = llvm_result;
            } else {
                llvm_result = LLVMBuildAdd(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                catalog->blocks[b].value_references[i] = llvm_result;    
            }
            break;
        case INSTRUCTION_FADD:
            llvm_result = LLVMBuildFAdd(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_SUBTRACT:
            // Do excess instructions for subtracting pointers to get llvm to shut up
            if(((ir_instr_math_t*) instr)->a->type->kind == TYPE_KIND_POINTER){
                LLVMValueRef val_a = ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a);
                LLVMValueRef val_b = ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b);
                val_a = LLVMBuildPtrToInt(builder, val_a, llvm->i64_type, "");
                val_b = LLVMBuildPtrToInt(builder, val_b, llvm->i64_type, "");
                llvm_result = LLVMBuildSub(builder, val_a, val_b, "");
                llvm_result = LLVMBuildIntToPtr(builder, llvm_result, ir_to_llvm_type(llvm, ((ir_instr_math_t*) instr)->a->type), "");
                catalog->blocks[b].value_references[i] = llvm_result;
            } else {
                llvm_result = LLVMBuildSub(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                catalog->blocks[b].value_references[i] = llvm_result;    
            }
            break;
        case INSTRUCTION_FSUBTRACT:
            llvm_result = LLVMBuildFSub(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_MULTIPLY:
            llvm_result = LLVMBuildMul(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_FMULTIPLY:
            llvm_result = LLVMBuildFMul(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_UDIVIDE:
            llvm_result = LLVMBuildUDiv(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_SDIVIDE:
            llvm_result = LLVMBuildSDiv(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_FDIVIDE:
            llvm_result = LLVMBuildFDiv(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_UMODULUS:
            llvm_result = LLVMBuildURem(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_SMODULUS:
            llvm_result = LLVMBuildSRem(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_FMODULUS:
            llvm_result = LLVMBuildFRem(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_CALL: {
                LLVMValueRef arguments[((ir_instr_call_t*) instr)->values_length];

                for(length_t v = 0; v != ((ir_instr_call_t*) instr)->values_length; v++){
                    arguments[v] = ir_to_llvm_value(llvm, ((ir_instr_call_t*) instr)->values[v]);
                }

                const char *implementation_name;
                char adept_implementation_name[256];
                ir_func_t *target_ir_func = &llvm->object->ir_module.funcs.funcs[((ir_instr_call_t*) instr)->ir_func_id];

                if(target_ir_func->traits & IR_FUNC_FOREIGN){
                    implementation_name = compiler_unnamespaced_name(target_ir_func->name);
                } else if(target_ir_func->traits & IR_FUNC_MAIN){
                    implementation_name = "main";
                } else if(target_ir_func->export_as){
                    implementation_name = target_ir_func->export_as;
                } else {
                    ir_implementation(((ir_instr_call_t*) instr)->ir_func_id, 'a', adept_implementation_name);
                    implementation_name = adept_implementation_name;
                }
                
                LLVMValueRef named_func = LLVMGetNamedFunction(llvm->module, implementation_name);
                assert(named_func != NULL);

                llvm_result = LLVMBuildCall(builder, named_func, arguments, ((ir_instr_call_t*) instr)->values_length, "");
                catalog->blocks[b].value_references[i] = llvm_result;
            }
            break;
        case INSTRUCTION_CALL_ADDRESS: {
                LLVMValueRef arguments[((ir_instr_call_address_t*) instr)->values_length];

                for(length_t v = 0; v != ((ir_instr_call_address_t*) instr)->values_length; v++){
                    arguments[v] = ir_to_llvm_value(llvm, ((ir_instr_call_address_t*) instr)->values[v]);
                }

                LLVMValueRef target_func = ir_to_llvm_value(llvm, ((ir_instr_call_address_t*) instr)->address);

                llvm_result = LLVMBuildCall(builder, target_func, arguments, ((ir_instr_call_address_t*) instr)->values_length, "");
                catalog->blocks[b].value_references[i] = llvm_result;
            }
            break;
        case INSTRUCTION_STORE: {
                ir_instr_store_t *store_instr = (ir_instr_store_t*) instr;
                LLVMValueRef destination = ir_to_llvm_value(llvm, store_instr->destination);
                ir_to_llvm_null_check(llvm, f, destination, store_instr->maybe_line_number, store_instr->maybe_column_number, &llvm_exit_blocks[b]);

                llvm_result = LLVMBuildStore(builder, ir_to_llvm_value(llvm, ((ir_instr_store_t*) instr)->value), destination);
                catalog->blocks[b].value_references[i] = llvm_result;
            }
            break;
        case INSTRUCTION_LOAD: {
                ir_instr_load_t *load_instr = (ir_instr_load_t*) instr;
                LLVMValueRef pointer = ir_to_llvm_value(llvm, load_instr->value);
                ir_to_llvm_null_check(llvm, f, pointer, load_instr->maybe_line_number, load_instr->maybe_column_number, &llvm_exit_blocks[b]);

                llvm_result = LLVMBuildLoad(builder, pointer, "");
                catalog->blocks[b].value_references[i] = llvm_result;
            }
            break;
        case INSTRUCTION_VARPTR:
            catalog->blocks[b].value_references[i] = llvm->stack->values[((ir_instr_varptr_t*) instr)->index];
            break;
        case INSTRUCTION_GLOBALVARPTR:
            catalog->blocks[b].value_references[i] = llvm->global_variables[((ir_instr_varptr_t*) instr)->index];
            break;
        case INSTRUCTION_STATICVARPTR:
            catalog->blocks[b].value_references[i] = llvm->static_variables.variables[((ir_instr_varptr_t*) instr)->index].global;
            break;
        case INSTRUCTION_BREAK:
            LLVMBuildBr(builder, llvm_blocks[((ir_instr_break_t*) instr)->block_id]);
            break;
        case INSTRUCTION_CONDBREAK:
            LLVMBuildCondBr(builder, ir_to_llvm_value(llvm, ((ir_instr_cond_break_t*) instr)->value), llvm_blocks[((ir_instr_cond_break_t*) instr)->true_block_id],
            llvm_blocks[((ir_instr_cond_break_t*) instr)->false_block_id]);
            break;
        case INSTRUCTION_EQUALS:
            llvm_result = LLVMBuildICmp(builder, LLVMIntEQ, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_FEQUALS:
            llvm_result = LLVMBuildFCmp(builder, LLVMRealOEQ, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_NOTEQUALS:
            llvm_result = LLVMBuildICmp(builder, LLVMIntNE, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_FNOTEQUALS:
            llvm_result = LLVMBuildFCmp(builder, LLVMRealONE, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_UGREATER:
            llvm_result = LLVMBuildICmp(builder, LLVMIntUGT, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_SGREATER:
            llvm_result = LLVMBuildICmp(builder, LLVMIntSGT, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_FGREATER:
            llvm_result = LLVMBuildFCmp(builder, LLVMRealOGT, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_ULESSER:
            catalog->blocks[b].value_references[i] = LLVMBuildICmp(builder, LLVMIntULT, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            break;
        case INSTRUCTION_SLESSER:
            llvm_result = LLVMBuildICmp(builder, LLVMIntSLT, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_FLESSER:
            llvm_result = LLVMBuildFCmp(builder, LLVMRealOLT, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_UGREATEREQ:
            llvm_result = LLVMBuildICmp(builder, LLVMIntUGE, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_SGREATEREQ:
            llvm_result = LLVMBuildICmp(builder, LLVMIntSGE, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_FGREATEREQ:
            llvm_result = LLVMBuildFCmp(builder, LLVMRealOGE, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_ULESSEREQ:
            llvm_result = LLVMBuildICmp(builder, LLVMIntULE, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_SLESSEREQ:
            llvm_result = LLVMBuildICmp(builder, LLVMIntSLE, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_FLESSEREQ:
            llvm_result = LLVMBuildFCmp(builder, LLVMRealOLE, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_MEMBER: {
                ir_instr_member_t *member_instr = (ir_instr_member_t*) instr;
                LLVMValueRef foundation = ir_to_llvm_value(llvm, member_instr->value);

                ir_to_llvm_null_check(llvm, f, foundation, member_instr->maybe_line_number, member_instr->maybe_column_number, &llvm_exit_blocks[b]);

                LLVMValueRef gep_indices[2];
                gep_indices[0] = LLVMConstInt(LLVMInt32Type(), 0, true);
                gep_indices[1] = LLVMConstInt(LLVMInt32Type(), ((ir_instr_member_t*) instr)->member, true);

                // For some reason, LLVM has problems with using a regular GEP for a constant value/indicies
                if(LLVMIsConstant(foundation)){
                    catalog->blocks[b].value_references[i] = LLVMConstGEP(foundation, gep_indices, 2);
                } else {
                    catalog->blocks[b].value_references[i] = LLVMBuildGEP(builder, foundation, gep_indices, 2, "");
                }
            }
            break;
        case INSTRUCTION_ARRAY_ACCESS: {
                ir_instr_array_access_t *array_access_instr = (ir_instr_array_access_t*) instr;
                LLVMValueRef foundation = ir_to_llvm_value(llvm, array_access_instr->value);

                ir_to_llvm_null_check(llvm, f, foundation, array_access_instr->maybe_line_number, array_access_instr->maybe_column_number, &llvm_exit_blocks[b]);

                LLVMValueRef gep_index = ir_to_llvm_value(llvm, array_access_instr->index);

                // For some reason, LLVM has problems with using a regular GEP for a constant value/indicies
                if(LLVMIsConstant(foundation) && LLVMIsConstant(gep_index)){
                    llvm_result = LLVMConstGEP(foundation, &gep_index, 1);
                } else {
                    llvm_result = LLVMBuildGEP(builder, foundation, &gep_index, 1, "");
                }

                catalog->blocks[b].value_references[i] = llvm_result;
            }
            break;
        case INSTRUCTION_FUNC_ADDRESS:
            if(((ir_instr_func_address_t*) instr)->name == NULL){
                if(llvm->object->ir_module.funcs.funcs[((ir_instr_func_address_t*) instr)->ir_func_id].export_as){
                    llvm_result = LLVMGetNamedFunction(llvm->module, llvm->object->ir_module.funcs.funcs[((ir_instr_func_address_t*) instr)->ir_func_id].export_as);
                } else {
                    // Not a foreign function, so resolve via id
                    char implementation_name[256];
                    ir_implementation(((ir_instr_func_address_t*) instr)->ir_func_id, 'a', implementation_name);
                    llvm_result = LLVMGetNamedFunction(llvm->module, implementation_name);
                }
            } else {
                // Is a foreign function, so get by name
                llvm_result = LLVMGetNamedFunction(llvm->module, ((ir_instr_func_address_t*) instr)->name);
            }

            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_BITCAST:
            llvm_result = LLVMBuildBitCast(builder, ir_to_llvm_value(llvm, ((ir_instr_cast_t*) instr)->value), ir_to_llvm_type(llvm, ((ir_instr_cast_t*) instr)->result_type), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_ZEXT:
            llvm_result = LLVMBuildZExt(builder, ir_to_llvm_value(llvm, ((ir_instr_cast_t*) instr)->value), ir_to_llvm_type(llvm, ((ir_instr_cast_t*) instr)->result_type), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_SEXT:
            llvm_result = LLVMBuildSExt(builder, ir_to_llvm_value(llvm, ((ir_instr_cast_t*) instr)->value), ir_to_llvm_type(llvm, ((ir_instr_cast_t*) instr)->result_type), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_FEXT:
            llvm_result = LLVMBuildFPExt(builder, ir_to_llvm_value(llvm, ((ir_instr_cast_t*) instr)->value), ir_to_llvm_type(llvm, ((ir_instr_cast_t*) instr)->result_type), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_TRUNC:
            llvm_result = LLVMBuildTrunc(builder, ir_to_llvm_value(llvm, ((ir_instr_cast_t*) instr)->value), ir_to_llvm_type(llvm, ((ir_instr_cast_t*) instr)->result_type), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_FTRUNC:
            llvm_result = LLVMBuildFPTrunc(builder, ir_to_llvm_value(llvm, ((ir_instr_cast_t*) instr)->value), ir_to_llvm_type(llvm, ((ir_instr_cast_t*) instr)->result_type), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_INTTOPTR:
            llvm_result = LLVMBuildIntToPtr(builder, ir_to_llvm_value(llvm, ((ir_instr_cast_t*) instr)->value), ir_to_llvm_type(llvm, ((ir_instr_cast_t*) instr)->result_type), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_PTRTOINT:
            llvm_result = LLVMBuildPtrToInt(builder, ir_to_llvm_value(llvm, ((ir_instr_cast_t*) instr)->value), ir_to_llvm_type(llvm, ((ir_instr_cast_t*) instr)->result_type), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_FPTOUI:
            llvm_result = LLVMBuildFPToUI(builder, ir_to_llvm_value(llvm, ((ir_instr_cast_t*) instr)->value), ir_to_llvm_type(llvm, ((ir_instr_cast_t*) instr)->result_type), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_FPTOSI:
            llvm_result = LLVMBuildFPToSI(builder, ir_to_llvm_value(llvm, ((ir_instr_cast_t*) instr)->value), ir_to_llvm_type(llvm, ((ir_instr_cast_t*) instr)->result_type), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_UITOFP:
            llvm_result = LLVMBuildUIToFP(builder, ir_to_llvm_value(llvm, ((ir_instr_cast_t*) instr)->value), ir_to_llvm_type(llvm, ((ir_instr_cast_t*) instr)->result_type), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_SITOFP:
            llvm_result = LLVMBuildSIToFP(builder, ir_to_llvm_value(llvm, ((ir_instr_cast_t*) instr)->value), ir_to_llvm_type(llvm, ((ir_instr_cast_t*) instr)->result_type), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_ISZERO: case INSTRUCTION_ISNTZERO: {
                unsigned int type_kind = ((ir_instr_cast_t*) instr)->value->type->kind;
                bool type_kind_is_float = (type_kind == TYPE_KIND_FLOAT || type_kind == TYPE_KIND_DOUBLE);
                LLVMValueRef zero;

                switch(type_kind){
                case TYPE_KIND_S8: zero = LLVMConstInt(LLVMInt8Type(), 0, true); break;
                case TYPE_KIND_U8: zero = LLVMConstInt(LLVMInt8Type(), 0, false); break;
                case TYPE_KIND_S16: zero = LLVMConstInt(LLVMInt16Type(), 0, true); break;
                case TYPE_KIND_U16: zero = LLVMConstInt(LLVMInt16Type(), 0, false); break;
                case TYPE_KIND_S32: zero = LLVMConstInt(LLVMInt32Type(), 0, true); break;
                case TYPE_KIND_U32: zero = LLVMConstInt(LLVMInt32Type(), 0, false); break;
                case TYPE_KIND_S64: zero = LLVMConstInt(llvm->i64_type, 0, true); break;
                case TYPE_KIND_U64: zero = LLVMConstInt(llvm->i64_type, 0, false); break;
                case TYPE_KIND_FLOAT: zero = LLVMConstReal(LLVMFloatType(), 0); break;
                case TYPE_KIND_DOUBLE: zero = LLVMConstReal(llvm->f64_type, 0); break;
                case TYPE_KIND_BOOLEAN: zero = LLVMConstInt(LLVMInt1Type(), 0, false); break;
                case TYPE_KIND_FUNCPTR:
                case TYPE_KIND_POINTER: zero = LLVMConstNull(ir_to_llvm_type(llvm, ((ir_instr_cast_t*) instr)->value->type)); break;
                default:
                    die("ir_to_llvm_instructions() - INSTRUCTION_ISxxZERO received unrecognized type kind\n");
                }

                bool isz = (instr->id == INSTRUCTION_ISZERO);
                llvm_result = ir_to_llvm_value(llvm, ((ir_instr_cast_t*) instr)->value);

                if(type_kind_is_float){
                    catalog->blocks[b].value_references[i] = LLVMBuildFCmp(builder, isz ? LLVMRealOEQ : LLVMRealONE, llvm_result, zero, "");
                } else {
                    catalog->blocks[b].value_references[i] = LLVMBuildICmp(builder, isz ? LLVMIntEQ : LLVMIntNE, llvm_result, zero, "");
                }
            }
            break;
        case INSTRUCTION_REINTERPRET:
            // Reinterprets a signed vs unsigned integer in higher level IR
            // LLVM Can ignore this instruction
            catalog->blocks[b].value_references[i] = ir_to_llvm_value(llvm, ((ir_instr_cast_t*) instr)->value);
            break;
        case INSTRUCTION_AND:
        case INSTRUCTION_BIT_AND:
            llvm_result = LLVMBuildAnd(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_OR:
        case INSTRUCTION_BIT_OR:
            llvm_result = LLVMBuildOr(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_SIZEOF: {
                length_t type_size = LLVMABISizeOfType(llvm->data_layout, ir_to_llvm_type(llvm, ((ir_instr_sizeof_t*) instr)->type));
                catalog->blocks[b].value_references[i] = LLVMConstInt(llvm->i64_type, type_size, false);
            }
            break;
        case INSTRUCTION_OFFSETOF: {
                unsigned long long offset = LLVMOffsetOfElement(llvm->data_layout, ir_to_llvm_type(llvm, ((ir_instr_offsetof_t*) instr)->type), ((ir_instr_offsetof_t*) instr)->index);
                catalog->blocks[b].value_references[i] = LLVMConstInt(llvm->i64_type, offset, false);
            }
            break;
        case INSTRUCTION_ZEROINIT: {
                ir_value_t *ir_value = ((ir_instr_zeroinit_t*) instr)->destination;
                LLVMValueRef destination = ir_to_llvm_value(llvm, ir_value);
                LLVMBuildStore(builder, LLVMConstNull(LLVMGetElementType(LLVMTypeOf(destination))), destination);
            }
            break;
        case INSTRUCTION_MALLOC: {
                ir_instr_malloc_t *malloc_instr = (ir_instr_malloc_t*) instr;
                LLVMTypeRef ty = ir_to_llvm_type(llvm, malloc_instr->type);
                LLVMValueRef allocated;

                if(malloc_instr->amount == NULL){    
                    allocated = LLVMBuildMalloc(builder, ty, "");
                    catalog->blocks[b].value_references[i] = allocated;
                    
                    if(!(malloc_instr->is_undef || llvm->compiler->traits & COMPILER_UNSAFE_NEW)){
                        LLVMBuildStore(builder, LLVMConstNull(ty), LLVMBuildBitCast(builder, allocated, LLVMPointerType(ty, 0), ""));
                    }
                } else {
                    LLVMValueRef count = ir_to_llvm_value(llvm, ((ir_instr_malloc_t*) instr)->amount);
                    allocated = LLVMBuildArrayMalloc(builder, ty, count, "");
                    catalog->blocks[b].value_references[i] = allocated;

                    if(!(malloc_instr->is_undef || llvm->compiler->traits & COMPILER_UNSAFE_NEW)){
                        LLVMValueRef *memset_intrinsic = &llvm->intrinsics.memset;

                        if(*memset_intrinsic == NULL){
                            LLVMTypeRef arg_types[4];
                            arg_types[0] = LLVMPointerType(LLVMInt8Type(), 0);
                            arg_types[1] = LLVMInt8Type();
                            arg_types[2] = llvm->i64_type;
                            arg_types[3] = LLVMInt1Type();

                            LLVMTypeRef memset_intrinsic_type = LLVMFunctionType(LLVMVoidType(), arg_types, 4, 0);
                            *memset_intrinsic = LLVMAddFunction(llvm->module, "llvm.memset.p0i8.i64", memset_intrinsic_type);
                        }

                        LLVMValueRef per_item_size = LLVMConstInt(llvm->i64_type, LLVMABISizeOfType(llvm->data_layout, ty), false);
                        count = LLVMBuildZExt(llvm->builder, count, llvm->i64_type, "");

                        LLVMValueRef args[4];
                        args[0] = LLVMBuildBitCast(llvm->builder, allocated, LLVMPointerType(LLVMInt8Type(), 0), "");
                        args[1] = LLVMConstInt(LLVMInt8Type(), 0, false);
                        args[2] = LLVMBuildMul(llvm->builder, per_item_size, count, "");
                        args[3] = LLVMConstInt(LLVMInt1Type(), 0, false);

                        LLVMBuildCall(builder, *memset_intrinsic, args, 4, "");
                    }
                }
            }
            break;
        case INSTRUCTION_FREE: {
                catalog->blocks[b].value_references[i] = LLVMBuildFree(builder, ir_to_llvm_value(llvm, ((ir_instr_free_t*) instr)->value));
            }
            break;
        case INSTRUCTION_MEMCPY: {
                LLVMValueRef *memcpy_intrinsic = &llvm->intrinsics.memcpy;

                if(*memcpy_intrinsic == NULL){
                    LLVMTypeRef arg_types[4];
                    arg_types[0] = LLVMPointerType(LLVMInt8Type(), 0);
                    arg_types[1] = LLVMPointerType(LLVMInt8Type(), 0);
                    arg_types[2] = llvm->i64_type;
                    arg_types[3] = LLVMInt1Type();

                    LLVMTypeRef memcpy_intrinsic_type = LLVMFunctionType(LLVMVoidType(), arg_types, 4, 0);
                    *memcpy_intrinsic = LLVMAddFunction(llvm->module, "llvm.memcpy.p0i8.p0i8.i64", memcpy_intrinsic_type);
                }

                LLVMValueRef args[4];
                args[0] = ir_to_llvm_value(llvm, ((ir_instr_memcpy_t*) instr)->destination);
                args[1] = ir_to_llvm_value(llvm, ((ir_instr_memcpy_t*) instr)->value);
                args[2] = ir_to_llvm_value(llvm, ((ir_instr_memcpy_t*) instr)->bytes);
                args[3] = LLVMConstInt(LLVMInt1Type(), ((ir_instr_memcpy_t*) instr)->is_volatile, false);

                LLVMBuildCall(builder, *memcpy_intrinsic, args, 4, "");
                catalog->blocks[b].value_references[i] = NULL;
            }
            break;
        case INSTRUCTION_BIT_XOR:
            llvm_result = LLVMBuildXor(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_BIT_LSHIFT:
            llvm_result = LLVMBuildShl(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_BIT_RSHIFT:
            llvm_result = LLVMBuildAShr(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_BIT_LGC_RSHIFT:
            llvm_result = LLVMBuildLShr(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_BIT_COMPLEMENT: {
                unsigned int type_kind = ((ir_instr_unary_t*) instr)->value->type->kind;
                LLVMValueRef base = ir_to_llvm_value(llvm, ((ir_instr_unary_t*) instr)->value);
                
                unsigned int bits = global_type_kind_sizes_in_bits_64[type_kind];
                LLVMValueRef transform = LLVMConstInt(LLVMIntType(bits), ~0, global_type_kind_signs[type_kind]);

                llvm_result = LLVMBuildXor(builder, base, transform, "");
                catalog->blocks[b].value_references[i] = llvm_result;
            }
            break;
        case INSTRUCTION_NEGATE: {
                LLVMValueRef base = ir_to_llvm_value(llvm, ((ir_instr_unary_t*) instr)->value);
                LLVMValueRef zero = LLVMConstNull(LLVMTypeOf(base));
                llvm_result = LLVMBuildSub(builder, zero, base, "");
                catalog->blocks[b].value_references[i] = llvm_result;
            }
            break;
        case INSTRUCTION_FNEGATE:
            llvm_result = LLVMBuildFNeg(builder, ir_to_llvm_value(llvm, ((ir_instr_unary_t*) instr)->value), "");
            catalog->blocks[b].value_references[i] = llvm_result;
            break;
        case INSTRUCTION_SELECT:
            catalog->blocks[b].value_references[i] = LLVMBuildSelect(builder,
                ir_to_llvm_value(llvm, ((ir_instr_select_t*) instr)->condition),
                ir_to_llvm_value(llvm, ((ir_instr_select_t*) instr)->if_true),
                ir_to_llvm_value(llvm, ((ir_instr_select_t*) instr)->if_false), "");
            break;
        case INSTRUCTION_PHI2: {
                LLVMValueRef phi = LLVMBuildPhi(llvm->builder, ir_to_llvm_type(llvm, instr->result_type), "");

                LLVMValueRef when_a = ir_to_llvm_value(llvm, ((ir_instr_phi2_t*) instr)->a);
                LLVMValueRef when_b = ir_to_llvm_value(llvm, ((ir_instr_phi2_t*) instr)->b);

                expand((void**) &llvm->relocation_list.unrelocated, sizeof(llvm_phi2_relocation_t), llvm->relocation_list.length, &llvm->relocation_list.capacity, 1, 4);
                llvm_phi2_relocation_t *delayed = &llvm->relocation_list.unrelocated[llvm->relocation_list.length++];
                delayed->phi = phi;
                delayed->a = when_a;
                delayed->b = when_b;
                delayed->basicblock_a = ((ir_instr_phi2_t*) instr)->block_id_a;
                delayed->basicblock_b = ((ir_instr_phi2_t*) instr)->block_id_b;

                catalog->blocks[b].value_references[i] = phi;
            }
            break;
        case INSTRUCTION_SWITCH: {
                LLVMValueRef value = ir_to_llvm_value(llvm, ((ir_instr_switch_t*) instr)->condition);
                LLVMValueRef switch_val = LLVMBuildSwitch(builder, value, llvm_blocks[((ir_instr_switch_t*) instr)->default_block_id], ((ir_instr_switch_t*) instr)->cases_length);
                
                for(length_t i = 0; i != ((ir_instr_switch_t*) instr)->cases_length; i++){
                    LLVMValueRef case_value = ir_to_llvm_value(llvm, ((ir_instr_switch_t*) instr)->case_values[i]);
                    LLVMAddCase(switch_val, case_value, llvm_blocks[((ir_instr_switch_t*) instr)->case_block_ids[i]]);
                }

                catalog->blocks[b].value_references[i] = NULL;
            }
            break;
        case INSTRUCTION_ALLOC: {
                ir_instr_alloc_t *alloc = (ir_instr_alloc_t*) instr;
                ir_type_t *target_result_type = alloc->result_type;

                if(target_result_type->kind != TYPE_KIND_POINTER){
                    die("ir_to_llvm_instructions() - INSTRUCTION_ALLOC has non-pointer result type\n");
                }

                catalog->blocks[b].value_references[i] = (alloc->count)
                    ? LLVMBuildArrayAlloca(llvm->builder, ir_to_llvm_type(llvm, target_result_type->extra), ir_to_llvm_value(llvm, alloc->count), "")
                    : LLVMBuildAlloca(llvm->builder, ir_to_llvm_type(llvm, target_result_type->extra), "");

                if(alloc->alignment != 0)
                    LLVMSetAlignment(catalog->blocks[b].value_references[i], alloc->alignment);
            }
            break;
        case INSTRUCTION_STACK_SAVE: {
                LLVMValueRef *stacksave_intrinsic = &llvm->intrinsics.stacksave;

                if(*stacksave_intrinsic == NULL){
                    LLVMTypeRef stacksave_intrinsic_type = LLVMFunctionType(LLVMPointerType(LLVMInt8Type(), 0), NULL, 0, 0);
                    *stacksave_intrinsic = LLVMAddFunction(llvm->module, "llvm.stacksave", stacksave_intrinsic_type);
                }

                catalog->blocks[b].value_references[i] = LLVMBuildCall(builder, *stacksave_intrinsic, NULL, 0, "");
            }
            break;
        case INSTRUCTION_STACK_RESTORE: {
                LLVMValueRef *stackrestore_intrinsic = &llvm->intrinsics.stackrestore;

                if(*stackrestore_intrinsic == NULL){
                    LLVMTypeRef arg_types[1];
                    arg_types[0] = LLVMPointerType(LLVMInt8Type(), 0);

                    LLVMTypeRef stackrestore_intrinsic_type = LLVMFunctionType(LLVMVoidType(), arg_types, 1, 0);
                    *stackrestore_intrinsic = LLVMAddFunction(llvm->module, "llvm.stackrestore", stackrestore_intrinsic_type);
                }

                LLVMValueRef args[1];
                args[0] = ir_to_llvm_value(llvm, ((ir_instr_unary_t*) instr)->value);

                LLVMBuildCall(builder, *stackrestore_intrinsic, args, 1, "");
                catalog->blocks[b].value_references[i] = NULL;
            }
            break;
        case INSTRUCTION_VA_START: case INSTRUCTION_VA_END: {
                bool is_start = instr->id == INSTRUCTION_VA_START;
                LLVMValueRef *va_intrinsic = is_start ? &llvm->intrinsics.va_start : &llvm->intrinsics.va_end;

                if(*va_intrinsic == NULL){
                    LLVMTypeRef params[] = {LLVMPointerType(LLVMInt8Type(), 0)};
                    LLVMTypeRef va_intrinsic_type = LLVMFunctionType(LLVMVoidType(), params, 1, 0);
                    *va_intrinsic = LLVMAddFunction(llvm->module, is_start ? "llvm.va_start" : "llvm.va_end", va_intrinsic_type);
                }

                LLVMValueRef args[1];
                args[0] = ir_to_llvm_value(llvm, ((ir_instr_unary_t*) instr)->value);
                
                LLVMBuildCall(builder, *va_intrinsic, args, 1, "");
                catalog->blocks[b].value_references[i] = NULL;
            }
            break;
        case INSTRUCTION_VA_ARG: {
                LLVMValueRef list = ir_to_llvm_value(llvm, ((ir_instr_va_arg_t*) instr)->va_list);
                LLVMTypeRef arg_type = ir_to_llvm_type(llvm, ((ir_instr_va_arg_t*) instr)->result_type);

                catalog->blocks[b].value_references[i] = LLVMBuildVAArg(builder, list, arg_type, "");
            }
            break;
        case INSTRUCTION_VA_COPY: {
                LLVMValueRef *va_copy_intrinsic = &llvm->intrinsics.va_copy;

                if(*va_copy_intrinsic == NULL){
                    LLVMTypeRef llvm_ptr_type = LLVMPointerType(LLVMInt8Type(), 0);
                    LLVMTypeRef params[] = {llvm_ptr_type, llvm_ptr_type};
                    LLVMTypeRef va_copy_intrinsic_type = LLVMFunctionType(LLVMVoidType(), params, 2, 0);
                    *va_copy_intrinsic = LLVMAddFunction(llvm->module, "llvm.va_copy", va_copy_intrinsic_type);
                }

                LLVMValueRef args[2];
                args[0] = ir_to_llvm_value(llvm, ((ir_instr_va_copy_t*) instr)->dest_value);
                args[1] = ir_to_llvm_value(llvm, ((ir_instr_va_copy_t*) instr)->src_value);
                
                LLVMBuildCall(builder, *va_copy_intrinsic, args, 2, "");
                catalog->blocks[b].value_references[i] = NULL;
            }
            break;
        case INSTRUCTION_ASM: {
                ir_instr_asm_t *asm_instr = (ir_instr_asm_t*) instr;
                LLVMValueRef *args = (LLVMValueRef*) malloc(sizeof(LLVMValueRef) * asm_instr->arity);
                LLVMTypeRef *types = (LLVMTypeRef*) malloc(sizeof(LLVMTypeRef) * asm_instr->arity);

                for(length_t a = 0; a != asm_instr->arity; a++){
                    args[a] = ir_to_llvm_value(llvm, asm_instr->args[a]);
                    types[a] = LLVMTypeOf(args[a]);
                }

                LLVMInlineAsmDialect dialect = asm_instr->is_intel ? LLVMInlineAsmDialectIntel : LLVMInlineAsmDialectATT;
                LLVMTypeRef fty = LLVMFunctionType(LLVMVoidType(), types, asm_instr->arity, false);

                LLVMValueRef inline_asm = LLVMGetInlineAsm(fty, asm_instr->assembly, 
                    strlen(asm_instr->assembly), asm_instr->constraints,
                    strlen(asm_instr->constraints), asm_instr->has_side_effects, asm_instr->is_stack_align, dialect

                    // TODO: Upgrade fully to LLVM 13+
                    #if LLVM_VERSION_MAJOR >= 12
                    , false
                    #endif
                );
                
                LLVMBuildCall(builder, inline_asm, args, asm_instr->arity, "");
                free(args);
                free(types);
            }
            break;
        case INSTRUCTION_DEINIT_SVARS:
            if(llvm->static_variable_info.deinit_function == NULL){
                die("ir_to_llvm_instructions() - INSTRUCTION_DEINIT_SVARS cannot operate since static_variables_deinitialization_function doesn't exist\n");
            }

            LLVMBuildCall(builder, llvm->static_variable_info.deinit_function, NULL, 0, "");
            break;
        case INSTRUCTION_UNREACHABLE:
            LLVMBuildUnreachable(builder);
            break;
        default:
            die("ir_to_llvm_instructions() - Unrecognized instruction '%d'\n", (int) instr->id);
        }
    }

    return SUCCESS;
}

errorcode_t ir_to_llvm_globals(llvm_context_t *llvm, object_t *object){
    ir_global_t *globals = object->ir_module.globals;
    length_t globals_length = object->ir_module.globals_length;
    ir_anon_global_t *anon_globals = object->ir_module.anon_globals.globals;
    length_t anon_globals_length = object->ir_module.anon_globals.length;

    LLVMModuleRef module = llvm->module;
    char global_implementation_name[256];

    for(length_t i = 0; i != anon_globals_length; i++){
        LLVMTypeRef anon_global_llvm_type = ir_to_llvm_type(llvm, anon_globals[i].type);
        llvm->anon_global_variables[i] = LLVMAddGlobal(module, anon_global_llvm_type, "");
        LLVMSetLinkage(llvm->anon_global_variables[i], LLVMPrivateLinkage);
        LLVMSetGlobalConstant(llvm->anon_global_variables[i], anon_globals[i].traits & IR_ANON_GLOBAL_CONSTANT);
    }

    for(length_t i = 0; i != anon_globals_length; i++){
        if(anon_globals[i].initializer == NULL) continue;
        if(!VALUE_TYPE_IS_CONSTANT(anon_globals[i].initializer->value_type)) continue;
        LLVMSetInitializer(llvm->anon_global_variables[i], ir_to_llvm_value(llvm, anon_globals[i].initializer));
    }

    for(length_t i = 0; i != globals_length; i++){
        bool is_external = globals[i].traits & IR_GLOBAL_EXTERNAL;
        LLVMTypeRef global_llvm_type = ir_to_llvm_type(llvm, globals[i].type);

        if(is_external)
            ir_implementation(i, 'g', global_implementation_name);

        llvm->global_variables[i] = LLVMAddGlobal(module, global_llvm_type, is_external ? globals[i].name : global_implementation_name);
        LLVMSetLinkage(llvm->global_variables[i], is_external ? LLVMExternalLinkage : LLVMInternalLinkage);

        if(globals[i].traits & IR_GLOBAL_THREAD_LOCAL)
            LLVMSetThreadLocal(llvm->global_variables[i], true);
        
        if(globals[i].trusted_static_initializer){
            // Non-user static value initializer
            // (Used for __types__ and __types_length__)
            LLVMSetInitializer(llvm->global_variables[i], ir_to_llvm_value(llvm, globals[i].trusted_static_initializer));
        } else if(!is_external){
            // In order to prevent the aggressive global elemination we'll perform later
            // from accidentally removing necessary user-defined global variables,
            // we'll manually mark which ones not to touch
            LLVMSetExternallyInitialized(llvm->global_variables[i], true);

            LLVMSetInitializer(llvm->global_variables[i], LLVMGetUndef(global_llvm_type));
        }
    }

    return SUCCESS;
}

void ir_to_llvm_null_check(llvm_context_t *llvm, length_t func_skeleton_index, LLVMValueRef pointer, int line, int column, LLVMBasicBlockRef *out_landing_basicblock){
    if(!(llvm->compiler->checks & COMPILER_NULL_CHECKS)) return;

    LLVMBasicBlockRef not_null_block = LLVMAppendBasicBlock(llvm->func_skeletons[func_skeleton_index], "");

    LLVMBasicBlockRef current_block = LLVMGetInsertBlock(llvm->builder);
    LLVMValueRef line_value = LLVMConstInt(LLVMInt32Type(), line, true);
    LLVMValueRef column_value = LLVMConstInt(LLVMInt32Type(), column, true);

    LLVMAddIncoming(llvm->null_check.line_phi, &line_value, &current_block, 1);
    LLVMAddIncoming(llvm->null_check.column_phi, &column_value, &current_block, 1);

    LLVMValueRef if_null = LLVMBuildIsNull(llvm->builder, pointer, "");
    LLVMBuildCondBr(llvm->builder, if_null, llvm->null_check.on_fail_block, not_null_block);
    LLVMPositionBuilderAtEnd(llvm->builder, not_null_block);

    // Set landing basicblock output to be the not-null case block
    if(out_landing_basicblock) *out_landing_basicblock = not_null_block;
}

LLVMCodeGenOptLevel ir_to_llvm_config_optlvl(compiler_t *compiler){
    switch(compiler->optimization){
    case OPTIMIZATION_NONE:       return LLVMCodeGenLevelNone;
    case OPTIMIZATION_LESS:       return LLVMCodeGenLevelLess;
    case OPTIMIZATION_DEFAULT:    return LLVMCodeGenLevelDefault;
    case OPTIMIZATION_AGGRESSIVE: return LLVMCodeGenLevelAggressive;
    default:                      return LLVMCodeGenLevelDefault;
    }
}

LLVMValueRef llvm_string_table_find(llvm_string_table_t *table, weak_cstr_t array, length_t length){
    // If not found returns NULL else returns global variable value

    maybe_index_t first, middle, last, comparison;
    first = 0; last = table->length - 1;

    llvm_string_table_entry_t target;
    target.data = array;
    target.length = length;
    // (neglect target.global_data)

    while(first <= last){
        middle = (first + last) / 2;
        comparison = llvm_string_table_entry_cmp(&table->entries[middle], &target);

        if(comparison == 0) return table->entries[middle].global_data;
        else if(comparison > 0) last = middle - 1;
        else first = middle + 1;
    }

    return NULL;
}

void llvm_string_table_add(llvm_string_table_t *table, weak_cstr_t name, length_t length, LLVMValueRef global_data){
    expand((void**) &table->entries, sizeof(llvm_string_table_entry_t), table->length, &table->capacity, 1, 64);

    llvm_string_table_entry_t entry;
    entry.data = name;
    entry.length = length;
    entry.global_data = global_data;

    length_t insert_position = find_insert_position(table->entries, table->length, llvm_string_table_entry_cmp, &entry, sizeof(llvm_string_table_entry_t));

    // Move other entries over, so that the new entry has space
    memmove(&table->entries[insert_position + 1], &table->entries[insert_position], sizeof(llvm_string_table_entry_t) * (table->length - insert_position));

    table->entries[insert_position] = entry;
    table->length++;
}

int llvm_string_table_entry_cmp(const void *va, const void *vb){
    const llvm_string_table_entry_t *a = (llvm_string_table_entry_t*) va;
    const llvm_string_table_entry_t *b = (llvm_string_table_entry_t*) vb;

    // Don't use '-' cause could overflow with large values
    if(a->length != b->length) return a->length < b->length ? 1 : -1;

    return strncmp(a->data, b->data, a->length);
}

void value_catalog_prepare(value_catalog_t *out_catalog, ir_basicblocks_t basicblocks){
    out_catalog->blocks = malloc(sizeof(value_catalog_block_t) * basicblocks.length);
    out_catalog->blocks_length = basicblocks.length;

    for(length_t block_index = 0; block_index != basicblocks.length; block_index++){
        out_catalog->blocks[block_index].value_references = malloc(sizeof(LLVMValueRef) * basicblocks.blocks[block_index].instructions.length);
    }
}

void value_catalog_free(value_catalog_t *catalog){
    for(length_t block_index = 0; block_index != catalog->blocks_length; block_index++){
        free(catalog->blocks[block_index].value_references);
    }
    free(catalog->blocks);
}

errorcode_t ir_to_llvm_inject_init_built(llvm_context_t *llvm){
    if(llvm->static_variable_info.init_routine == NULL) return SUCCESS;

    object_t *object = llvm->object;
    ir_builder_t *init_builder = object->ir_module.init_builder;

    LLVMBuilderRef builder = LLVMCreateBuilder();
    ir_basicblocks_t basicblocks = init_builder->basicblocks;

    if(!llvm->object->ir_module.common.has_main){
        LLVMPositionBuilderAtEnd(builder, llvm->static_variable_info.init_routine);
        LLVMBuildBr(builder, llvm->static_variable_info.init_post);
        LLVMDisposeBuilder(builder);
        return SUCCESS;
    }

    value_catalog_t catalog;
    value_catalog_prepare(&catalog, basicblocks);

    varstack_t stack;
    stack.values = NULL;
    stack.types = NULL;
    stack.length = 0;

    llvm->builder = builder;
    llvm->catalog = &catalog;
    llvm->stack = &stack;

    length_t f = object->ir_module.common.ir_main_id;
    ir_func_t *module_func = &object->ir_module.funcs.funcs[f];

    LLVMBasicBlockRef *llvm_blocks = malloc(sizeof(LLVMBasicBlockRef) * basicblocks.length);
    LLVMValueRef func_skeleton = llvm->func_skeletons[f];

    // If the true exit point of a block changed, its real value will be in here
    // (Used for PHI instructions to have the proper end point)
    LLVMBasicBlockRef *llvm_exit_blocks = malloc(sizeof(LLVMBasicBlockRef) * basicblocks.length);

    // Information about PHI instructions that need to have their incoming blocks delayed
    llvm->relocation_list.length = 0;

    // Create basicblocks
    for(length_t b = 0; b != basicblocks.length; b++){
        llvm_blocks[b] = LLVMAppendBasicBlock(func_skeleton, "");
        llvm_exit_blocks[b] = llvm_blocks[b];
    }

    // Drop references to any old PHIs
    llvm->null_check.line_phi = NULL;
    llvm->null_check.column_phi = NULL;

    errorcode_t errorcode = ir_to_llvm_basicblocks(llvm, basicblocks, func_skeleton,
            module_func, llvm_blocks, llvm_exit_blocks, f);

    if(errorcode == SUCCESS){
        LLVMPositionBuilderAtEnd(builder, llvm->static_variable_info.init_routine);
    
        if(basicblocks.length != 0){
            LLVMBuildBr(builder, llvm_blocks[0]);
            LLVMPositionBuilderAtEnd(builder, llvm_exit_blocks[basicblocks.length - 1]);
        }

        LLVMBuildBr(builder, llvm->static_variable_info.init_post);
    }
    
    value_catalog_free(&catalog);
    free(stack.values);
    free(stack.types);
    free(llvm_blocks);
    free(llvm_exit_blocks);
    LLVMDisposeBuilder(builder);
    return errorcode;
}

errorcode_t ir_to_llvm_inject_deinit_built(llvm_context_t *llvm){
    if(llvm->static_variable_info.deinit_function == NULL) return SUCCESS;

    // REFACTOR:
    // TODO: Refactor this to abstract out the parts needed
    // by 'ir_to_llvm_inject_init_built' vs 'ir_to_llvm_inject_deinit_built'

    object_t *object = llvm->object;
    ir_builder_t *deinit_builder = object->ir_module.deinit_builder;

    LLVMBuilderRef builder = LLVMCreateBuilder();
    ir_basicblocks_t basicblocks = deinit_builder->basicblocks;

    if(llvm->static_variable_info.deinit_function == NULL){
        internalerrorprintf("ir_to_llvm_inject_deinit_built() - static_variables_deinitialization_function does not exist\n");
        return FAILURE;
    }

    value_catalog_t catalog;
    value_catalog_prepare(&catalog, basicblocks);

    varstack_t stack;
    stack.values = NULL;
    stack.types = NULL;
    stack.length = 0;

    llvm->builder = builder;
    llvm->catalog = &catalog;
    llvm->stack = &stack;

    length_t f = object->ir_module.common.ir_main_id;
    ir_func_t *module_func = &object->ir_module.funcs.funcs[f];

    LLVMBasicBlockRef *llvm_blocks = malloc(sizeof(LLVMBasicBlockRef) * basicblocks.length);
    LLVMValueRef func_skeleton = llvm->static_variable_info.deinit_function;

    // If the true exit point of a block changed, its real value will be in here
    // (Used for PHI instructions to have the proper end point)
    LLVMBasicBlockRef *llvm_exit_blocks = malloc(sizeof(LLVMBasicBlockRef) * basicblocks.length);

    // Information about PHI instructions that need to have their incoming blocks delayed
    llvm->relocation_list.length = 0;

    // Create basicblocks
    for(length_t b = 0; b != basicblocks.length; b++){
        llvm_blocks[b] = LLVMAppendBasicBlock(func_skeleton, "");
        llvm_exit_blocks[b] = llvm_blocks[b];
    }

    // Drop references to any old PHIs
    llvm->null_check.line_phi = NULL;
    llvm->null_check.column_phi = NULL;

    errorcode_t errorcode = ir_to_llvm_basicblocks(llvm, basicblocks, func_skeleton,
            module_func, llvm_blocks, llvm_exit_blocks, f);

    if(errorcode == SUCCESS){
        LLVMBuildRetVoid(builder);
    }

    value_catalog_free(&catalog);
    free(stack.values);
    free(stack.types);
    free(llvm_blocks);
    free(llvm_exit_blocks);
    LLVMDisposeBuilder(builder);
    return errorcode;
}

errorcode_t ir_to_llvm_generate_deinit_svars_function_head(llvm_context_t *llvm){
    LLVMValueRef *deinit_function = &llvm->static_variable_info.deinit_function;

    if(*deinit_function != NULL){
        internalerrorprintf("ir_to_llvm_generate_deinit_svars_function_head() - Static variable deinitialization function already exists\n");
        return FAILURE;
    }

    // Create head of function that will deinitialize static variables
    LLVMTypeRef fty = LLVMFunctionType(LLVMVoidType(), NULL, 0, false);
    *deinit_function = LLVMAddFunction(llvm->module, "____adeinitsvars", fty);
    LLVMSetLinkage(*deinit_function, LLVMPrivateLinkage);
    return SUCCESS;
}
