
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include "ir.h"
#include "color.h"
#include "object.h"
#include "filename.h"
#include "ir_to_llvm.h"

LLVMTypeRef ir_to_llvm_type(ir_type_t *ir_type){
    // Converts an ir type to an llvm type
    LLVMTypeRef type_ref_tmp;

    switch(ir_type->kind){
    case TYPE_KIND_POINTER:
        type_ref_tmp = ir_to_llvm_type((ir_type_t*) ir_type->extra);
        if(type_ref_tmp == NULL) return NULL;
        return LLVMPointerType(type_ref_tmp, 0);
    case TYPE_KIND_S8:      return LLVMInt8Type();
    case TYPE_KIND_S16:     return LLVMInt16Type();
    case TYPE_KIND_S32:     return LLVMInt32Type();
    case TYPE_KIND_S64:     return LLVMInt64Type();
    case TYPE_KIND_U8:      return LLVMInt8Type();
    case TYPE_KIND_U16:     return LLVMInt16Type();
    case TYPE_KIND_U32:     return LLVMInt32Type();
    case TYPE_KIND_U64:     return LLVMInt64Type();
    case TYPE_KIND_HALF:    return LLVMHalfType();
    case TYPE_KIND_FLOAT:   return LLVMFloatType();
    case TYPE_KIND_DOUBLE:  return LLVMDoubleType();
    case TYPE_KIND_BOOLEAN: return LLVMInt1Type();
    case TYPE_KIND_UNION:
        printf("INTERNAL ERROR: TYPE_KIND_UNION not implemented yet inside ir_to_llvm_type\n");
        return NULL;
    case TYPE_KIND_STRUCTURE: {
            // TODO: Should probably cache struct types so they don't have to be
            //           remade into LLVM types every time we use them.

            ir_type_extra_composite_t *composite = (ir_type_extra_composite_t*) ir_type->extra;
            LLVMTypeRef *fields = malloc(sizeof(LLVMTypeRef) * composite->subtypes_length);

            for(length_t i = 0; i != composite->subtypes_length; i++){
                fields[i] = ir_to_llvm_type(composite->subtypes[i]);
                if(fields[i] == NULL){ // Escape if error
                    free(fields);
                    return NULL;
                }
            }

            LLVMTypeRef type_ref_tmp = LLVMStructType(fields, composite->subtypes_length,
                composite->traits & TYPE_KIND_COMPOSITE_PACKED);

            free(fields);
            return type_ref_tmp;
        }
    case TYPE_KIND_VOID: return LLVMVoidType();
    case TYPE_KIND_FUNCPTR: {
            ir_type_extra_function_t *function = (ir_type_extra_function_t*) ir_type->extra;
            LLVMTypeRef *args = malloc(sizeof(LLVMTypeRef) * function->arity);

            for(length_t i = 0; i != function->arity; i++){
                args[i] = ir_to_llvm_type(function->arg_types[i]);
                if(args[i] == NULL){ // Escape if error
                    free(args);
                    return NULL;
                }
            }

            LLVMTypeRef type_ref_tmp = LLVMFunctionType(ir_to_llvm_type(function->return_type),
                args, function->arity, function->traits & TYPE_KIND_FUNC_VARARG);
            type_ref_tmp = LLVMPointerType(type_ref_tmp, 0);

            free(args);
            return type_ref_tmp;
        }
        break;
    default: return NULL; // No suitable llvm type
    }
}

LLVMValueRef ir_to_llvm_value(llvm_context_t *llvm, ir_value_t *value){
    // Retrieves a literal or previously computed value

    if(value == NULL){
        redprintf("INTERNAL ERROR: ir_to_llvm_value() got NULL pointer\n");
        return NULL;
    }

    switch(value->value_type){
    case VALUE_TYPE_LITERAL: {
            switch(value->type->kind){
            case TYPE_KIND_S8: return LLVMConstInt(LLVMInt8Type(), *((char*) value->extra), true);
            case TYPE_KIND_U8: return LLVMConstInt(LLVMInt8Type(), *((unsigned char*) value->extra), false);
            case TYPE_KIND_S16: return LLVMConstInt(LLVMInt16Type(), *((int*) value->extra), true);
            case TYPE_KIND_U16: return LLVMConstInt(LLVMInt16Type(), *((unsigned int*) value->extra), false);
            case TYPE_KIND_S32: return LLVMConstInt(LLVMInt32Type(), *((long long*) value->extra), true);
            case TYPE_KIND_U32: return LLVMConstInt(LLVMInt32Type(), *((unsigned long long*) value->extra), false);
            case TYPE_KIND_S64: return LLVMConstInt(LLVMInt64Type(), *((long long*) value->extra), true);
            case TYPE_KIND_U64: return LLVMConstInt(LLVMInt64Type(), *((unsigned long long*) value->extra), false);
            case TYPE_KIND_FLOAT: return LLVMConstReal(LLVMFloatType(), *((double*) value->extra));
            case TYPE_KIND_DOUBLE: return LLVMConstReal(LLVMDoubleType(), *((double*) value->extra));
            case TYPE_KIND_BOOLEAN: return LLVMConstInt(LLVMInt1Type(), *((bool*) value->extra), false);
            case TYPE_KIND_POINTER:
                if( ((ir_type_t*) value->type->extra)->kind == TYPE_KIND_U8 ){ // Null-terminated string literal
                    char *data = (char*) value->extra;
                    length_t data_length = strlen((char*) value->extra) + 1;
                    LLVMValueRef global_data = LLVMAddGlobal(llvm->module, LLVMArrayType(LLVMInt8Type(), data_length), ".str");
                    LLVMSetLinkage(global_data, LLVMInternalLinkage);
                    LLVMSetGlobalConstant(global_data, true);
                    LLVMSetInitializer(global_data, LLVMConstString(data, data_length, true));
                    LLVMValueRef indices[2];
                    indices[0] = LLVMConstInt(LLVMInt32Type(), 0, true);
                    indices[1] = LLVMConstInt(LLVMInt32Type(), 0, true);
                    LLVMValueRef literal = LLVMBuildGEP(llvm->builder, global_data, indices, 2, "");
                    return literal;
                }
                // Fall through to default if not caught
            default:
                redprintf("INTERNAL ERROR: Unknown type kind literal in ir_to_llvm_value\n");
                return NULL;
            }
        }
    case VALUE_TYPE_RESULT: {
            ir_value_result_t *extra = (ir_value_result_t*) value->extra;
            return llvm->catalog->blocks[extra->block_id].value_references[extra->instruction_id];
        }
    case VALUE_TYPE_NULLPTR:
        return LLVMConstNull(LLVMPointerType(LLVMInt8Type(), 0));
    default:
        redprintf("INTERNAL ERROR: Unknown value type %d of value in ir_to_llvm_value\n", value->value_type);
        return NULL;
    }

    return NULL;
}

int ir_to_llvm_functions(llvm_context_t *llvm, object_t *object){
    // Generates llvm function skeletons from ir function data

    LLVMModuleRef llvm_module = llvm->module;
    ir_func_t *funcs = object->ir_module.funcs;
    length_t funcs_length = object->ir_module.funcs_length;
    LLVMValueRef *func_skeletons = llvm->func_skeletons;

    for(length_t f = 0; f != funcs_length; f++){
        LLVMTypeRef *parameters = malloc(sizeof(LLVMTypeRef) * funcs[f].arity);

        for(length_t a = 0; a != funcs[f].arity; a++){
            parameters[a] = ir_to_llvm_type(funcs[f].argument_types[a]);
            if(parameters[a] == NULL){
                free(parameters);
                return 1;
            }
        }

        LLVMTypeRef return_type = ir_to_llvm_type(funcs[f].return_type);
        LLVMTypeRef llvm_func_type = LLVMFunctionType(return_type, parameters, funcs[f].arity, funcs[f].traits & IR_FUNC_VARARG);

        const char *implementation_name;

        if(funcs[f].traits & IR_FUNC_FOREIGN || object->ast.funcs[f].traits & AST_FUNC_MAIN){
            implementation_name = funcs[f].name;
        } else {
            char adept_implementation_name[256];
            sprintf(adept_implementation_name, "adept_%X", (int) f);
            implementation_name = adept_implementation_name;
        }

        func_skeletons[f] = LLVMAddFunction(llvm_module, implementation_name, llvm_func_type);

        LLVMCallConv call_conv = funcs[f].traits & IR_FUNC_STDCALL ? LLVMX86StdcallCallConv : LLVMCCallConv;
        LLVMSetFunctionCallConv(func_skeletons[f], call_conv);

        free(parameters);
    }

    return 0;
}

int ir_to_llvm_function_bodies(llvm_context_t *llvm, object_t *object){
    // Generates llvm function bodies from ir function data
    // NOTE: Expects function skeltons to already be present

    LLVMModuleRef llvm_module = llvm->module;
    ir_func_t *funcs = object->ir_module.funcs;
    length_t funcs_length = object->ir_module.funcs_length;
    LLVMValueRef *func_skeletons = llvm->func_skeletons;

    for(length_t f = 0; f != funcs_length; f++){
        LLVMBuilderRef builder = LLVMCreateBuilder();
        ir_basicblock_t *basicblocks = funcs[f].basicblocks;
        length_t basicblocks_length = funcs[f].basicblocks_length;

        value_catalog_t catalog;
        catalog.blocks = malloc(sizeof(value_catalog_block_t) * basicblocks_length);
        catalog.blocks_length = basicblocks_length;
        for(length_t c = 0; c != basicblocks_length; c++) catalog.blocks[c].value_references = malloc(sizeof(LLVMValueRef) * basicblocks[c].instructions_length);

        stack_t stack;
        stack.values = malloc(sizeof(LLVMValueRef) * funcs[f].var_map.mappings_length);
        stack.length = funcs[f].var_map.mappings_length;

        llvm->module = llvm_module;
        llvm->builder = builder;
        llvm->catalog = &catalog;
        llvm->stack = &stack;

        LLVMBasicBlockRef *llvm_blocks = malloc(sizeof(LLVMBasicBlockRef) * basicblocks_length);
        ir_instr_t *instr;
        LLVMValueRef llvm_result;

        for(length_t b = 0; b != basicblocks_length; b++) llvm_blocks[b] = LLVMAppendBasicBlock(func_skeletons[f], "");

        for(length_t b = 0; b != basicblocks_length; b++){
            LLVMPositionBuilderAtEnd(builder, llvm_blocks[b]);
            ir_basicblock_t *basicblock = &basicblocks[b];

            if(b == 0){ // Do any function entry instructions needed
                ir_var_mapping_t *var_mappings = funcs[f].var_map.mappings;

                // Allocate stack variables
                for(length_t s = 0; s != stack.length; s++){
                    LLVMTypeRef alloca_type = ir_to_llvm_type(var_mappings[s].var.type);

                    if(alloca_type == NULL){
                        for(length_t c = 0; c != catalog.blocks_length; c++) free(catalog.blocks[c].value_references);
                        free(catalog.blocks);
                        free(stack.values);
                        free(llvm_blocks);
                        LLVMDisposeBuilder(builder);
                        return 1;
                    }

                    stack.values[s] = LLVMBuildAlloca(builder, alloca_type, "");

                    if(s < funcs[f].arity){
                        // Function argument that needs passed argument value
                        LLVMBuildStore(builder, LLVMGetParam(func_skeletons[f], s), stack.values[s]);
                    } else if(!(var_mappings[s].var.traits & IR_VAR_UNDEF)){
                        // User declared variable that needs to be auto-initalized
                        LLVMBuildStore(builder, LLVMConstNull(alloca_type), stack.values[s]);
                    }
                }
            }

            for(length_t i = 0; i != basicblock->instructions_length; i++){
                switch(basicblock->instructions[i]->id){
                case INSTRUCTION_RET:
                    instr = basicblock->instructions[i];
                    LLVMBuildRet(builder, ((ir_instr_ret_t*) instr)->value == NULL ? NULL : ir_to_llvm_value(llvm, ((ir_instr_ret_t*) instr)->value));
                    break;
                case INSTRUCTION_ADD:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildAdd(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_FADD:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildFAdd(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_SUBTRACT:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildSub(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                    break;
                case INSTRUCTION_FSUBTRACT:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildFSub(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_MULTIPLY:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildMul(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                    break;
                case INSTRUCTION_FMULTIPLY:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildFMul(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_UDIVIDE:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildUDiv(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_SDIVIDE:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildSDiv(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_FDIVIDE:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildFDiv(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_UMODULUS:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildURem(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_SMODULUS:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildSRem(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_FMODULUS:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildFRem(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_CALL: {
                        instr = basicblocks[b].instructions[i];
                        LLVMValueRef *arguments = malloc(sizeof(LLVMValueRef) * ((ir_instr_call_t*) instr)->values_length);

                        for(length_t v = 0; v != ((ir_instr_call_t*) instr)->values_length; v++){
                            arguments[v] = ir_to_llvm_value(llvm, ((ir_instr_call_t*) instr)->values[v]);
                        }

                        char *implementation_name;
                        ast_func_t *target_ast_func = &object->ast.funcs[((ir_instr_call_t*) instr)->func_id];

                        if(target_ast_func->traits & AST_FUNC_FOREIGN || target_ast_func->traits & AST_FUNC_MAIN){
                            implementation_name = target_ast_func->name;
                        } else {
                            char adept_implementation_name[256];
                            sprintf(adept_implementation_name, "adept_%X", (int) ((ir_instr_call_t*) instr)->func_id);
                            implementation_name = adept_implementation_name;
                        }

                        LLVMValueRef named_func = LLVMGetNamedFunction(llvm_module, implementation_name);
                        assert(named_func != NULL);

                        llvm_result = LLVMBuildCall(builder, named_func, arguments, ((ir_instr_call_t*) instr)->values_length, "");
                        catalog.blocks[b].value_references[i] = llvm_result;
                        free(arguments);
                    }
                    break;
                case INSTRUCTION_CALL_ADDRESS: {
                        instr = basicblocks[b].instructions[i];
                        LLVMValueRef *arguments = malloc(sizeof(LLVMValueRef) * ((ir_instr_call_address_t*) instr)->values_length);

                        for(length_t v = 0; v != ((ir_instr_call_address_t*) instr)->values_length; v++){
                            arguments[v] = ir_to_llvm_value(llvm, ((ir_instr_call_address_t*) instr)->values[v]);
                        }

                        LLVMValueRef target_func = ir_to_llvm_value(llvm, ((ir_instr_call_address_t*) instr)->address);

                        llvm_result = LLVMBuildCall(builder, target_func, arguments, ((ir_instr_call_address_t*) instr)->values_length, "");
                        catalog.blocks[b].value_references[i] = llvm_result;
                        free(arguments);
                    }
                    break;
                case INSTRUCTION_STORE:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildStore(builder, ir_to_llvm_value(llvm, ((ir_instr_store_t*) instr)->value), ir_to_llvm_value(llvm, ((ir_instr_store_t*) instr)->destination));
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_LOAD:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildLoad(builder, ir_to_llvm_value(llvm, ((ir_instr_load_t*) instr)->value), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_VARPTR:
                    catalog.blocks[b].value_references[i] = llvm->stack->values[((ir_instr_varptr_t*) basicblock->instructions[i])->index];
                    break;
                case INSTRUCTION_GLOBALVARPTR:
                    catalog.blocks[b].value_references[i] = llvm->global_variables[((ir_instr_varptr_t*) basicblock->instructions[i])->index];
                    break;
                case INSTRUCTION_BREAK:
                    LLVMBuildBr(builder, llvm_blocks[((ir_instr_break_t*) basicblock->instructions[i])->block_id]);
                    break;
                case INSTRUCTION_CONDBREAK:
                    instr = basicblock->instructions[i];
                    LLVMBuildCondBr(builder, ir_to_llvm_value(llvm, ((ir_instr_cond_break_t*) instr)->value), llvm_blocks[((ir_instr_cond_break_t*) instr)->true_block_id],
                    llvm_blocks[((ir_instr_cond_break_t*) instr)->false_block_id]);
                    break;
                case INSTRUCTION_EQUALS:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildICmp(builder, LLVMIntEQ, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_FEQUALS:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildFCmp(builder, LLVMRealOEQ, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_NOTEQUALS:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildICmp(builder, LLVMIntNE, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_FNOTEQUALS:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildFCmp(builder, LLVMRealONE, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_UGREATER:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildICmp(builder, LLVMIntUGT, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_SGREATER:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildICmp(builder, LLVMIntSGT, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_FGREATER:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildFCmp(builder, LLVMRealOGT, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_ULESSER:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildICmp(builder, LLVMIntULT, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_SLESSER:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildICmp(builder, LLVMIntSLT, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_FLESSER:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildFCmp(builder, LLVMRealOLT, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_UGREATEREQ:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildICmp(builder, LLVMIntUGE, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_SGREATEREQ:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildICmp(builder, LLVMIntSGE, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_FGREATEREQ:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildFCmp(builder, LLVMRealOGE, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_ULESSEREQ:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildICmp(builder, LLVMIntULE, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_SLESSEREQ:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildICmp(builder, LLVMIntSLE, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_FLESSEREQ:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildFCmp(builder, LLVMRealOLE, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_MEMBER: {
                        instr = basicblock->instructions[i];
                        LLVMValueRef gep_indices[2];
                        gep_indices[0] = LLVMConstInt(LLVMInt32Type(), 0, true);
                        gep_indices[1] = LLVMConstInt(LLVMInt32Type(), ((ir_instr_member_t*) instr)->member, true);
                        llvm_result = LLVMBuildGEP(builder, ir_to_llvm_value(llvm, ((ir_instr_member_t*) instr)->value), gep_indices, 2, "");
                        catalog.blocks[b].value_references[i] = llvm_result;
                    }
                    break;
                case INSTRUCTION_ARRAY_ACCESS: {
                        instr = basicblock->instructions[i];
                        LLVMValueRef gep_index = ir_to_llvm_value(llvm, ((ir_instr_array_access_t*) instr)->index);
                        llvm_result = LLVMBuildGEP(builder, ir_to_llvm_value(llvm, ((ir_instr_array_access_t*) instr)->value), &gep_index, 1, "");
                        catalog.blocks[b].value_references[i] = llvm_result;
                    }
                    break;
                case INSTRUCTION_FUNC_ADDRESS:
                    instr = basicblock->instructions[i];

                    if(((ir_instr_func_address_t*) instr)->name == NULL){
                        // Not a foreign function, so resolve via id
                        char implementation_name[256];
                        sprintf(implementation_name, "adept_%X", (int) ((ir_instr_func_address_t*) instr)->func_id);
                        llvm_result = LLVMGetNamedFunction(llvm_module, implementation_name);
                    } else {
                        // Is a foreign function, so get by name
                        llvm_result = LLVMGetNamedFunction(llvm_module, ((ir_instr_func_address_t*) instr)->name);
                    }

                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_BITCAST:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildBitCast(builder, ir_to_llvm_value(llvm, ((ir_instr_cast_t*) instr)->value), ir_to_llvm_type(((ir_instr_cast_t*) instr)->result_type), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_ZEXT:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildZExt(builder, ir_to_llvm_value(llvm, ((ir_instr_cast_t*) instr)->value), ir_to_llvm_type(((ir_instr_cast_t*) instr)->result_type), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_FEXT:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildFPExt(builder, ir_to_llvm_value(llvm, ((ir_instr_cast_t*) instr)->value), ir_to_llvm_type(((ir_instr_cast_t*) instr)->result_type), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_TRUNC:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildTrunc(builder, ir_to_llvm_value(llvm, ((ir_instr_cast_t*) instr)->value), ir_to_llvm_type(((ir_instr_cast_t*) instr)->result_type), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_FTRUNC:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildFPTrunc(builder, ir_to_llvm_value(llvm, ((ir_instr_cast_t*) instr)->value), ir_to_llvm_type(((ir_instr_cast_t*) instr)->result_type), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_INTTOPTR:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildIntToPtr(builder, ir_to_llvm_value(llvm, ((ir_instr_cast_t*) instr)->value), ir_to_llvm_type(((ir_instr_cast_t*) instr)->result_type), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_PTRTOINT:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildPtrToInt(builder, ir_to_llvm_value(llvm, ((ir_instr_cast_t*) instr)->value), ir_to_llvm_type(((ir_instr_cast_t*) instr)->result_type), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_FPTOUI:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildFPToUI(builder, ir_to_llvm_value(llvm, ((ir_instr_cast_t*) instr)->value), ir_to_llvm_type(((ir_instr_cast_t*) instr)->result_type), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_FPTOSI:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildFPToSI(builder, ir_to_llvm_value(llvm, ((ir_instr_cast_t*) instr)->value), ir_to_llvm_type(((ir_instr_cast_t*) instr)->result_type), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_UITOFP:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildUIToFP(builder, ir_to_llvm_value(llvm, ((ir_instr_cast_t*) instr)->value), ir_to_llvm_type(((ir_instr_cast_t*) instr)->result_type), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_SITOFP:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildSIToFP(builder, ir_to_llvm_value(llvm, ((ir_instr_cast_t*) instr)->value), ir_to_llvm_type(((ir_instr_cast_t*) instr)->result_type), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_ISZERO: case INSTRUCTION_ISNTZERO: {
                        instr = basicblock->instructions[i];

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
                        case TYPE_KIND_S64: zero = LLVMConstInt(LLVMInt64Type(), 0, true); break;
                        case TYPE_KIND_U64: zero = LLVMConstInt(LLVMInt64Type(), 0, false); break;
                        case TYPE_KIND_FLOAT: zero = LLVMConstReal(LLVMFloatType(), 0); break;
                        case TYPE_KIND_DOUBLE: zero = LLVMConstReal(LLVMDoubleType(), 0); break;
                        case TYPE_KIND_BOOLEAN: zero = LLVMConstInt(LLVMInt1Type(), 0, false); break;
                        case TYPE_KIND_POINTER: zero = LLVMConstNull(ir_to_llvm_type(((ir_instr_cast_t*) instr)->value->type)); break;
                        default:
                            redprintf("INTERNAL ERROR: INSTRUCTION_ISNTZERO received unknown type kind\n");
                            for(length_t c = 0; c != catalog.blocks_length; c++) free(catalog.blocks[c].value_references);
                            free(catalog.blocks);
                            free(stack.values);
                            free(llvm_blocks);
                            LLVMDisposeBuilder(builder);
                            return 1;
                        }

                        bool isz = (basicblock->instructions[i]->id == INSTRUCTION_ISZERO);
                        llvm_result = ir_to_llvm_value(llvm, ((ir_instr_cast_t*) instr)->value);

                        if(type_kind_is_float){
                            catalog.blocks[b].value_references[i] = LLVMBuildFCmp(builder, isz ? LLVMRealOEQ : LLVMRealONE, llvm_result, zero, "");
                        } else {
                            catalog.blocks[b].value_references[i] = LLVMBuildICmp(builder, isz ? LLVMIntEQ : LLVMIntNE, llvm_result, zero, "");
                        }
                    }
                    break;
                case INSTRUCTION_REINTERPRET:
                    // Reinterprets a signed vs unsigned integer in higher level IR
                    // LLVM Can ignore this instruction
                    instr = basicblock->instructions[i];
                    catalog.blocks[b].value_references[i] = ir_to_llvm_value(llvm, ((ir_instr_cast_t*) instr)->value);
                    break;
                case INSTRUCTION_AND:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildAnd(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_OR:
                    instr = basicblock->instructions[i];
                    llvm_result = LLVMBuildOr(builder, ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->a), ir_to_llvm_value(llvm, ((ir_instr_math_t*) instr)->b), "");
                    catalog.blocks[b].value_references[i] = llvm_result;
                    break;
                case INSTRUCTION_SIZEOF: {
                        instr = basicblock->instructions[i];
                        length_t type_size = LLVMABISizeOfType(llvm->data_layout, ir_to_llvm_type(((ir_instr_sizeof_t*) instr)->type));
                        catalog.blocks[b].value_references[i] = LLVMConstInt(LLVMInt64Type(), type_size, false);
                    }
                    break;
                case INSTRUCTION_MALLOC: {
                        instr = basicblock->instructions[i];
                        if( ((ir_instr_malloc_t*) instr)->amount == NULL ){
                            catalog.blocks[b].value_references[i] = LLVMBuildMalloc(builder, ir_to_llvm_type(((ir_instr_malloc_t*) instr)->type), "");
                        } else {
                            catalog.blocks[b].value_references[i] = LLVMBuildArrayMalloc(builder,
                                ir_to_llvm_type(((ir_instr_malloc_t*) instr)->type), ir_to_llvm_value(llvm, ((ir_instr_malloc_t*) instr)->amount), "");
                        }
                    }
                    break;
                case INSTRUCTION_FREE: {
                        instr = basicblock->instructions[i];
                        catalog.blocks[b].value_references[i] = LLVMBuildFree(builder, ir_to_llvm_value(llvm, ((ir_instr_free_t*) instr)->value));
                    }
                    break;
                default:
                    redprintf("INTERNAL ERROR: Unexpected instruction '%d' when exporting ir to llvm\n", basicblocks[b].instructions[i]->id);
                    for(length_t c = 0; c != catalog.blocks_length; c++) free(catalog.blocks[c].value_references);
                    free(catalog.blocks);
                    free(stack.values);
                    free(llvm_blocks);
                    LLVMDisposeBuilder(builder);
                    return 1;
                }
            }
        }

        for(length_t c = 0; c != catalog.blocks_length; c++) free(catalog.blocks[c].value_references);
        free(catalog.blocks);
        free(stack.values);
        free(llvm_blocks);
        LLVMDisposeBuilder(builder);
    }
    return 0;
}

int ir_to_llvm_globals(llvm_context_t *llvm, object_t *object){
    ir_global_t *globals = object->ir_module.globals;
    length_t globals_length = object->ir_module.globals_length;

    LLVMModuleRef module = llvm->module;
    char global_implementation_name[256];

    for(length_t i = 0; i != globals_length; i++){
        LLVMTypeRef global_llvm_type = ir_to_llvm_type(globals[i].type);

        sprintf(global_implementation_name, "adeptglob_%X", (int) i);

        llvm->global_variables[i] = LLVMAddGlobal(module, global_llvm_type, global_implementation_name);
        LLVMSetLinkage(llvm->global_variables[i], LLVMExternalLinkage);
        LLVMSetInitializer(llvm->global_variables[i], LLVMGetUndef(global_llvm_type));
    }

    return 0;
}

int ir_to_llvm(compiler_t *compiler, object_t *object){
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();

    ir_module_t *module = &object->ir_module;
    llvm_context_t llvm;

    llvm.module = LLVMModuleCreateWithName(filename_name_const(object->filename));
    LLVMVerifyModule(llvm.module, LLVMAbortProcessAction, NULL);

    char *triple = "x86_64-pc-windows-gnu";
    LLVMSetTarget(llvm.module, triple);

    char *error_message; LLVMTargetRef target;
    if(LLVMGetTargetFromTriple(triple, &target, &error_message)){
        redprintf("INTERNAL ERROR: LLVMGetTargetFromTriple failed: %s\n", error_message);
        LLVMDisposeMessage(error_message);
        return 1;
    }

    if(compiler->output_filename == NULL){
        // May need to change based on operating system etc.
        compiler->output_filename = filename_ext(object->filename, "exe");
    } else {
        // Automatically add proper extension if missing
        filename_auto_ext(&compiler->output_filename, FILENAME_AUTO_EXECUTABLE);
    }

    char* object_filename = filename_ext(compiler->output_filename, "o");

    char *cpu = "generic";
    char *features = "";
    LLVMCodeGenOptLevel level = ir_to_llvm_config_optlvl(compiler);
    LLVMRelocMode reloc = LLVMRelocDefault;
    LLVMCodeModel code_model = LLVMCodeModelDefault;
    LLVMTargetMachineRef target_machine = LLVMCreateTargetMachine(target, triple, cpu, features, level, reloc, code_model);

    LLVMTargetDataRef data_layout = LLVMCreateTargetDataLayout(target_machine);
    LLVMSetModuleDataLayout(llvm.module, data_layout);
    llvm.data_layout = data_layout;

    llvm.func_skeletons = malloc(sizeof(LLVMValueRef) * module->funcs_length);
    llvm.global_variables = malloc(sizeof(LLVMValueRef) * module->globals_length);

    if(ir_to_llvm_globals(&llvm, object) || ir_to_llvm_functions(&llvm, object)
    || ir_to_llvm_function_bodies(&llvm, object)){
        free(object_filename);
        free(llvm.func_skeletons);
        free(llvm.global_variables);
        LLVMDisposeTargetMachine(target_machine);
        return 1;
    }

    #ifdef ENABLE_DEBUG_FEATURES
    if(compiler->debug_traits & COMPILER_DEBUG_LLVMIR) LLVMDumpModule(llvm.module);
    #endif // ENABLE_DEBUG_FEATURES

    // Free reference arrays
    free(llvm.func_skeletons);
    free(llvm.global_variables);

    // TODO: SECURITY: Stop using system(1) call to invoke linker
    const char *linker = "C:/Adept/2.0/mingw64/bin/x86_64-w64-mingw32-gcc"; // May need to change depending on system etc.
    length_t linker_length = strlen(linker);

    const char *linker_options = "-Wl,--start-group";
    length_t linker_options_length = strlen(linker_options);

    char *linker_additional = "";
    length_t linker_additional_length = 0;
    length_t linker_additional_index = 0;

    for(length_t i = 0; i != object->ast.libraries_length; i++)
        linker_additional_length += strlen(object->ast.libraries[i]) + 3;

    if(linker_additional_length != 0){
        linker_additional = malloc(linker_additional_length + 1);
        for(length_t i = 0; i != object->ast.libraries_length; i++){
            linker_additional[linker_additional_index++] = ' ';
            linker_additional[linker_additional_index++] = '\"';
            length_t lib_length = strlen(object->ast.libraries[i]);
            memcpy(&linker_additional[linker_additional_index], object->ast.libraries[i], lib_length);
            linker_additional_index += lib_length;
            linker_additional[linker_additional_index++] = '\"';
        }
        linker_additional[linker_additional_index] = '\0';
    }

    // linker + " \"" + object_filename + "\" -o " + compiler->output_filename + "\""
    char *gcc_link_command = malloc(linker_length + 1 + linker_options_length + linker_additional_length + 2 + strlen(object_filename) + 6 + strlen(compiler->output_filename) + 2);
    sprintf(gcc_link_command, "%s %s%s \"%s\" -o \"%s\"", linker, linker_options, linker_additional, object_filename, compiler->output_filename);

    if(linker_additional_length != 0) free(linker_additional);

    LLVMPassManagerRef pass_manager = LLVMCreatePassManager();
    LLVMCodeGenFileType codegen = LLVMObjectFile;

    if(LLVMTargetMachineEmitToFile(target_machine, llvm.module, object_filename, codegen, &error_message)){
        redprintf("INTERNAL ERROR: LLVMTargetMachineEmitToFile failed: %s\n", error_message);
        LLVMDisposeTargetMachine(target_machine);
        LLVMDisposeMessage(error_message);
        free(object_filename);
    }

    LLVMRunPassManager(pass_manager, llvm.module);
    LLVMDisposeTargetMachine(target_machine);
    LLVMDisposePassManager(pass_manager);

    if(system(gcc_link_command) != 0){
        redprintf("EXTERNAL ERROR: gcc link command failed\n%s\n", gcc_link_command);
        free(object_filename);
        free(gcc_link_command);
        return 1;
    }

    if(compiler->traits & COMPILER_EXECUTE_RESULT) system(compiler->output_filename);

    free(object_filename);
    free(gcc_link_command);
    return 0;
}

LLVMCodeGenOptLevel ir_to_llvm_config_optlvl(compiler_t *compiler){
    switch(compiler->optimization){
    case OPTIMIZATION_NONE:       return LLVMCodeGenLevelNone;
    case OPTIMIZATION_LESS:       return LLVMCodeGenLevelLess;
    case OPTIMIZATION_DEFAULT:    return LLVMCodeGenLevelDefault;
    case OPTIMIZATION_AGGRESSIVE: return LLVMCodeGenLevelAggressive;
    default: return LLVMCodeGenLevelDefault;
    }
}
