
#include "IR/ir_dump.h"

#include <stdlib.h>

#include "BRIDGE/bridge.h"
#include "IR/ir_type.h"
#include "IR/ir_value.h"
#include "IR/ir_value_str.h"
#include "UTIL/color.h"
#include "UTIL/string.h"

void ir_module_dump(ir_module_t *ir_module, const char *filename){
    FILE *file = fopen(filename, "w");

    if(file == NULL){
        die("ir_module_dump() - Failed to open ir module dump file\n");
    }

    ir_dump_functions(file, &ir_module->funcs);
    fclose(file);
}

static length_t count_instructions(const ir_basicblocks_t *basicblocks){
    length_t total = 0;

    for(length_t b = 0; b != basicblocks->length; b++){
        total += basicblocks->blocks[b].instructions.length;
    }

    return total;
}

static void ir_dump_var_scope_layout(FILE *file, bridge_scope_t *scope){
    if(scope == NULL) return;

    for(length_t i = 0; i != scope->list.length; i++){
        strong_cstr_t type_str = ir_type_str(scope->list.variables[i].ir_type);
        fprintf(file, "    [0x%08X %s]\n", (int) i, type_str);
        free(type_str);
    }

    for(length_t i = 0; i != scope->children_length; i++){
        ir_dump_var_scope_layout(file, scope->children[i]);
    }
}

void ir_dump_function(FILE *file, ir_func_t *function, funcid_t ir_func_id, ir_func_t *all_funcs){
    char mangled_name[32];
    ir_implementation(ir_func_id, 'a', mangled_name);

    // Print prototype
    strong_cstr_t return_type_str = ir_type_str(function->return_type);
    fprintf(file, "fn %s %s -> %s\n", function->name, mangled_name, return_type_str);
    free(return_type_str);

    // Count the total number of instructions
    length_t total_instructions = count_instructions(&function->basicblocks);

    // Print functions statistics
    if(!(function->traits & IR_FUNC_FOREIGN)){
        fprintf(file, "    {%zu BBs, %zu INSTRs, %zu VARs}\n", function->basicblocks.length, total_instructions, function->variable_count);
    }
    
    // Print stack variables
    ir_dump_var_scope_layout(file, function->scope);

    // Print instructions
    ir_dump_basicsblocks(file, function->basicblocks, all_funcs);
}

void ir_dump_functions(FILE *file, ir_funcs_t *funcs){
    for(length_t i = 0; i != funcs->length; i++){
        ir_dump_function(file, &funcs->funcs[i], i, funcs->funcs);
    }
}

void ir_dump_basicsblocks(FILE *file, ir_basicblocks_t basicblocks, ir_func_t *all_funcs){
    for(length_t b = 0; b != basicblocks.length; b++){
        ir_instrs_t *instructions = &basicblocks.blocks[b].instructions;

        fprintf(file, "  BASICBLOCK |%zu|\n", b);

        for(length_t i = 0; i != instructions->length; i++){
            ir_dump_instruction(file, instructions->instructions[i], i, all_funcs);
        }
    }
}

static void ir_dump_return_instruction(FILE *file, ir_instr_ret_t *instruction){
    ir_value_t *value = instruction->value;

    if(value != NULL){
        strong_cstr_t value_str = ir_value_str(value);
        fprintf(file, "ret %s\n", value_str);
        free(value_str);
    } else {
        fprintf(file, "ret\n");
    }
}

static void ir_dump_malloc_instruction(FILE *file, ir_instr_malloc_t *instruction){
    strong_cstr_t typename = ir_type_str(instruction->type);

    if(instruction->amount == NULL){
        fprintf(file, "malloc %s\n", typename);
    } else {
        strong_cstr_t amount = ir_value_str(instruction->amount);
        fprintf(file, "malloc %s * %s\n", typename, amount);
        free(amount);
    }

    free(typename);
}

static void ir_dump_free_instruction(FILE *file, ir_instr_free_t *instruction){
    strong_cstr_t value_str = ir_value_str(instruction->value);
    fprintf(file, "free %s\n", value_str);
    free(value_str);
}

static void ir_dump_store_instruction(FILE *file, ir_instr_store_t *instruction){
    strong_cstr_t value_str = ir_value_str(instruction->value);
    strong_cstr_t destination_str = ir_value_str(instruction->destination);

    fprintf(file, "store %s, %s\n", destination_str, value_str);

    free(value_str);
    free(destination_str);
}

static void ir_dump_load_instruction(FILE *file, ir_instr_load_t *instruction){
    strong_cstr_t value_str = ir_value_str(instruction->value);
    fprintf(file, "load %s\n", value_str);
    free(value_str);
}

static void ir_dump_varptr_instruction(FILE *file, ir_instr_varptr_t *instruction, const char *kind_name){
    strong_cstr_t result_type_str = ir_type_str(instruction->result_type);
    fprintf(file, "%s %s 0x%08X\n", kind_name, result_type_str, (int) instruction->index);
    free(result_type_str);
}

static void ir_dump_condbreak_instruction(FILE *file, ir_instr_cond_break_t *instruction){
    strong_cstr_t value_str = ir_value_str(instruction->value);
    fprintf(file, "cbr %s, |%zu|, |%zu|\n", value_str, instruction->true_block_id, instruction->false_block_id);
    free(value_str);
}

static void ir_dump_member_instruction(FILE *file, ir_instr_member_t *instruction){
    strong_cstr_t value_str = ir_value_str(instruction->value);
    fprintf(file, "memb %s, %zu\n", value_str, instruction->member);
    free(value_str);
}

static void ir_dump_array_access(FILE *file, ir_instr_array_access_t *instruction){
    strong_cstr_t value_str = ir_value_str(instruction->value);
    strong_cstr_t index_str = ir_value_str(instruction->index);
    fprintf(file, "arracc %s, %s\n", value_str, index_str);
    free(value_str);
    free(index_str);
}

static void ir_dump_function_address(FILE *file, ir_instr_func_address_t *instruction){
    if(instruction->name == NULL){
        char buffer[32];
        ir_implementation(instruction->ir_func_id, 0x00, buffer);
        fprintf(file, "funcaddr 0x%s\n", buffer);
    } else {
        fprintf(file, "funcaddr %s\n", instruction->name);
    }
}

static void ir_dump_cast_instruction(FILE *file, ir_instr_cast_t *instruction, const char *instr_name){
    strong_cstr_t to_type = ir_type_str(instruction->result_type);
    strong_cstr_t value_str = ir_value_str(instruction->value);

    fprintf(file, "%s %s to %s\n", instr_name, value_str, to_type);

    free(to_type);
    free(value_str);
}

static void ir_dump_zero_condition_instruction(FILE *file, ir_instr_unary_t *instruction, const char *kind_name){
    strong_cstr_t value_str = ir_value_str(instruction->value);

    fprintf(file, "%s %s\n", kind_name, value_str);

    free(value_str);
}

static void ir_dump_sizeof_instruction(FILE *file, ir_instr_sizeof_t *instruction){
    strong_cstr_t typename = ir_type_str(instruction->type);
    fprintf(file, "sizeof %s\n", typename);
    free(typename);
}

static void ir_dump_offsetof_instruction(FILE *file, ir_instr_offsetof_t *instruction){
    strong_cstr_t typename = ir_type_str(instruction->type);
    fprintf(file, "offsetof %s . %zu\n", typename, instruction->index);
    free(typename);
}

static void ir_dump_zeroinit_instruction(FILE *file, ir_instr_zeroinit_t *instruction){
    strong_cstr_t value_str = ir_value_str(((ir_instr_zeroinit_t*) instruction)->destination);
    fprintf(file, "zi %s\n", value_str);
    free(value_str);
}

static void ir_dump_memcpy_instruction(FILE *file, ir_instr_memcpy_t *instruction){
    strong_cstr_t destination = ir_value_str(instruction->destination);
    strong_cstr_t value = ir_value_str(instruction->value);
    strong_cstr_t bytes = ir_value_str(instruction->bytes);

    fprintf(file, "memcpy %s, %s, %s\n", destination, value, bytes);

    free(destination);
    free(value);
    free(bytes);
}

static void ir_dump_unary_instruction(FILE *file, ir_instr_unary_t *instruction, const char *kind_name){
    strong_cstr_t value_str = ir_value_str(instruction->value);
    fprintf(file, "%s %s\n", kind_name, value_str);
    free(value_str);
}

static void ir_dump_phi2_instruction(FILE *file, ir_instr_phi2_t *instruction){
    strong_cstr_t a = ir_value_str(instruction->a);
    strong_cstr_t b = ir_value_str(instruction->b);
    fprintf(file, "phi2 |%zu| -> %s, |%zu| -> %s\n", instruction->block_id_a, a, instruction->block_id_b, b);
    free(a);
    free(b);
}

static void ir_dump_switch_instruction(FILE *file, ir_instr_switch_t *instruction){
    strong_cstr_t cond = ir_value_str(instruction->condition);
    fprintf(file, "switch %s\n", cond);
    free(cond);

    for(length_t i = 0; i != instruction->cases_length; i++){
        strong_cstr_t case_value_str = ir_value_str(instruction->case_values[i]);
        fprintf(file, "               (%s) -> |%zu|\n", case_value_str, instruction->case_block_ids[i]);
        free(case_value_str);
    }

    if(instruction->default_block_id != instruction->resume_block_id){
        // Default case exists
        fprintf(file, "               default -> |%zu|\n", instruction->default_block_id);
    }
}

static void ir_dump_alloc_instruction(FILE *file, ir_instr_alloc_t *instruction){
    if(instruction->result_type->kind == TYPE_KIND_POINTER){
        strong_cstr_t typename = ir_type_str(instruction->result_type->extra);
        strong_cstr_t count = instruction->count ? ir_value_str(instruction->count) : strclone("single");
        fprintf(file, "alloc %s, aligned %d, count %s\n", typename, instruction->alignment, count);
        free(count);
        free(typename);
    } else {
        fprintf(file, "alloc (malformed)\n");
    }
}

static void ir_dump_va_arg_instruction(FILE *file, ir_instr_va_arg_t *instruction){
    strong_cstr_t value_str = ir_value_str(instruction->va_list);
    strong_cstr_t type_str = ir_type_str(instruction->result_type);

    fprintf(file, "va_arg %s, %s\n", value_str, type_str);
    
    free(value_str);
    free(type_str);
}

static void ir_dump_va_copy_instruction(FILE *file, ir_instr_va_copy_t *instruction){
    strong_cstr_t dest_str = ir_value_str(instruction->dest_value);
    strong_cstr_t value_str = ir_value_str(instruction->src_value);

    fprintf(file, "va_copy %s, %s\n", dest_str, value_str);
    
    free(value_str);
    free(dest_str);
}

static void ir_dump_math_instruction(FILE *file, ir_instr_math_t *instruction, const char *instruction_name){
    strong_cstr_t val_str_1 = ir_value_str(instruction->a);
    strong_cstr_t val_str_2 = ir_value_str(instruction->b);
    fprintf(file, "%s %s, %s\n", instruction_name, val_str_1, val_str_2);
    free(val_str_1);
    free(val_str_2);
}

static void ir_dump_call_instruction(FILE *file, ir_instr_call_t *instruction, ir_func_t *all_funcs){
    const char *real_name = all_funcs[instruction->ir_func_id].name;
    strong_cstr_t args = ir_values_str(instruction->values, instruction->values_length);
    strong_cstr_t result_type = ir_type_str(instruction->result_type);

    char mangled_name[32];
    ir_implementation((int) instruction->ir_func_id, 'a', mangled_name);

    fprintf(file, "call %s \"%s\" (%s) %s\n", mangled_name, real_name, args, result_type);

    free(args);
    free(result_type);
}

static void ir_dump_call_address_instruction(FILE *file, ir_instr_call_address_t *instruction){
    strong_cstr_t address = ir_value_str(instruction->address);
    strong_cstr_t args = ir_values_str(instruction->values, instruction->values_length);
    strong_cstr_t result_type = ir_type_str(instruction->result_type);

    fprintf(file, "calladdr %s(%s) %s\n", address, args, result_type);

    free(address);
    free(args);
    free(result_type);
}

void ir_dump_instruction(FILE *file, ir_instr_t *instruction, length_t instr_index, ir_func_t *all_funcs){
    fprintf(file, "    0x%08X ", (int) instr_index);

    switch(instruction->id){
    case INSTRUCTION_RET:
        ir_dump_return_instruction(file, (ir_instr_ret_t*) instruction);
        break;
    case INSTRUCTION_ADD:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "add");
        break;
    case INSTRUCTION_FADD:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "fadd");
        break;
    case INSTRUCTION_SUBTRACT:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "sub");
        break;
    case INSTRUCTION_FSUBTRACT:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "fsub");
        break;
    case INSTRUCTION_MULTIPLY:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "mul");
        break;
    case INSTRUCTION_FMULTIPLY:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "fmul");
        break;
    case INSTRUCTION_UDIVIDE:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "udiv");
        break;
    case INSTRUCTION_SDIVIDE:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "sdiv");
        break;
    case INSTRUCTION_FDIVIDE:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "fdiv");
        break;
    case INSTRUCTION_UMODULUS:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "urem");
        break;
    case INSTRUCTION_SMODULUS:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "srem");
        break;
    case INSTRUCTION_FMODULUS:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "frem");
        break;
    case INSTRUCTION_CALL:
        ir_dump_call_instruction(file, (ir_instr_call_t*) instruction, all_funcs);
        break;
    case INSTRUCTION_CALL_ADDRESS:
        ir_dump_call_address_instruction(file, (ir_instr_call_address_t*) instruction);
        break;
    case INSTRUCTION_MALLOC:
        ir_dump_malloc_instruction(file, (ir_instr_malloc_t*) instruction);
        break;
    case INSTRUCTION_FREE:
        ir_dump_free_instruction(file, (ir_instr_free_t*) instruction);
        break;
    case INSTRUCTION_STORE:
        ir_dump_store_instruction(file, (ir_instr_store_t*) instruction);
        break;
    case INSTRUCTION_LOAD:
        ir_dump_load_instruction(file, (ir_instr_load_t*) instruction);
        break;
    case INSTRUCTION_VARPTR:
        ir_dump_varptr_instruction(file, (ir_instr_varptr_t*) instruction, "var");
        break;
    case INSTRUCTION_GLOBALVARPTR:
        ir_dump_varptr_instruction(file, (ir_instr_varptr_t*) instruction, "gvar");
        break;
    case INSTRUCTION_STATICVARPTR:
        ir_dump_varptr_instruction(file, (ir_instr_varptr_t*) instruction, "svar");
        break;
    case INSTRUCTION_BREAK:
        fprintf(file, "br |%zu|\n", ((ir_instr_break_t*) instruction)->block_id);
        break;
    case INSTRUCTION_CONDBREAK:
        ir_dump_condbreak_instruction(file, (ir_instr_cond_break_t*) instruction);
        break;
    case INSTRUCTION_EQUALS:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "eq");
        break;
    case INSTRUCTION_FEQUALS:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "feq");
        break;
    case INSTRUCTION_NOTEQUALS:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "neq");
        break;
    case INSTRUCTION_FNOTEQUALS:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "fneq");
        break;
    case INSTRUCTION_UGREATER:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "ugt");
        break;
    case INSTRUCTION_SGREATER:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "sgt");
        break;
    case INSTRUCTION_FGREATER:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "fgt");
        break;
    case INSTRUCTION_ULESSER:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "ult");
        break;
    case INSTRUCTION_SLESSER:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "slt");
        break;
    case INSTRUCTION_FLESSER:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "flt");
        break;
    case INSTRUCTION_UGREATEREQ:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "uge");
        break;
    case INSTRUCTION_SGREATEREQ:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "sge");
        break;
    case INSTRUCTION_FGREATEREQ:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "fge");
        break;
    case INSTRUCTION_ULESSEREQ:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "ule");
        break;
    case INSTRUCTION_SLESSEREQ:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "sle");
        break;
    case INSTRUCTION_FLESSEREQ:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "fle");
        break;
    case INSTRUCTION_MEMBER:
        ir_dump_member_instruction(file, (ir_instr_member_t*) instruction);
        break;
    case INSTRUCTION_ARRAY_ACCESS:
        ir_dump_array_access(file, (ir_instr_array_access_t*) instruction);
        break;
    case INSTRUCTION_FUNC_ADDRESS:
        ir_dump_function_address(file, (ir_instr_func_address_t*) instruction);
        break;
    case INSTRUCTION_BITCAST:
        ir_dump_cast_instruction(file, (ir_instr_cast_t*) instruction, "bc");
        break;
    case INSTRUCTION_ZEXT:
        ir_dump_cast_instruction(file, (ir_instr_cast_t*) instruction, "zext");
        break;
    case INSTRUCTION_SEXT:
        ir_dump_cast_instruction(file, (ir_instr_cast_t*) instruction, "sext");
        break;
    case INSTRUCTION_FEXT:
        ir_dump_cast_instruction(file, (ir_instr_cast_t*) instruction, "fext");
        break;
    case INSTRUCTION_TRUNC:
        ir_dump_cast_instruction(file, (ir_instr_cast_t*) instruction, "trunc");
        break;
    case INSTRUCTION_FTRUNC:
        ir_dump_cast_instruction(file, (ir_instr_cast_t*) instruction, "ftrunc");
        break;
    case INSTRUCTION_INTTOPTR:
        ir_dump_cast_instruction(file, (ir_instr_cast_t*) instruction, "i2p");
        break;
    case INSTRUCTION_PTRTOINT:
        ir_dump_cast_instruction(file, (ir_instr_cast_t*) instruction, "p2i");
        break;
    case INSTRUCTION_FPTOUI:
        ir_dump_cast_instruction(file, (ir_instr_cast_t*) instruction, "fp2ui");
        break;
    case INSTRUCTION_FPTOSI:
        ir_dump_cast_instruction(file, (ir_instr_cast_t*) instruction, "fp2si");
        break;
    case INSTRUCTION_UITOFP:
        ir_dump_cast_instruction(file, (ir_instr_cast_t*) instruction, "ui2fp");
        break;
    case INSTRUCTION_SITOFP:
        ir_dump_cast_instruction(file, (ir_instr_cast_t*) instruction, "si2fp");
        break;
    case INSTRUCTION_REINTERPRET:
        ir_dump_cast_instruction(file, (ir_instr_cast_t*) instruction, "reinterp");
        break;
    case INSTRUCTION_ISZERO:
        ir_dump_zero_condition_instruction(file, (ir_instr_unary_t*) instruction, "isz");
        break;
    case INSTRUCTION_ISNTZERO:
        ir_dump_zero_condition_instruction(file, (ir_instr_unary_t*) instruction, "inz");
        break;
    case INSTRUCTION_AND:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "and");
        break;
    case INSTRUCTION_OR:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "or");
        break;
    case INSTRUCTION_SIZEOF:
        ir_dump_sizeof_instruction(file, (ir_instr_sizeof_t*) instruction);
        break;
    case INSTRUCTION_OFFSETOF:
        ir_dump_offsetof_instruction(file, (ir_instr_offsetof_t*) instruction);
        break;
    case INSTRUCTION_ZEROINIT:
        ir_dump_zeroinit_instruction(file, (ir_instr_zeroinit_t*) instruction);
        break;
    case INSTRUCTION_MEMCPY:
        ir_dump_memcpy_instruction(file, (ir_instr_memcpy_t*) instruction);
        break;
    case INSTRUCTION_BIT_AND:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "band");
        break;
    case INSTRUCTION_BIT_OR:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "bor");
        break;
    case INSTRUCTION_BIT_XOR:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "bxor");
        break;
    case INSTRUCTION_BIT_LSHIFT:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "lshift");
        break;
    case INSTRUCTION_BIT_RSHIFT:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "rshift");
        break;
    case INSTRUCTION_BIT_LGC_RSHIFT:
        ir_dump_math_instruction(file, (ir_instr_math_t*) instruction, "lgcrshift");
        break;
    case INSTRUCTION_BIT_COMPLEMENT:
        ir_dump_unary_instruction(file, (ir_instr_unary_t*) instruction, "compl");
        break;
    case INSTRUCTION_NEGATE:
        ir_dump_unary_instruction(file, (ir_instr_unary_t*) instruction, "neg");
        break;
    case INSTRUCTION_FNEGATE:
        ir_dump_unary_instruction(file, (ir_instr_unary_t*) instruction, "fneg");
        break;
    case INSTRUCTION_PHI2:
        ir_dump_phi2_instruction(file, (ir_instr_phi2_t*) instruction);
        break;
    case INSTRUCTION_SWITCH:
        ir_dump_switch_instruction(file, (ir_instr_switch_t*) instruction);
        break;
    case INSTRUCTION_ALLOC:
        ir_dump_alloc_instruction(file, (ir_instr_alloc_t*) instruction);
        break;
    case INSTRUCTION_STACK_SAVE:
        fprintf(file, "ssave\n");
        break;
    case INSTRUCTION_STACK_RESTORE:
        ir_dump_unary_instruction(file, (ir_instr_unary_t*) instruction, "srestore");
        break;
    case INSTRUCTION_VA_START:
        ir_dump_unary_instruction(file, (ir_instr_unary_t*) instruction, "va_start");
        break;
    case INSTRUCTION_VA_END:
        ir_dump_unary_instruction(file, (ir_instr_unary_t*) instruction, "va_end");
        break;
    case INSTRUCTION_VA_ARG:
        ir_dump_va_arg_instruction(file, (ir_instr_va_arg_t*) instruction);
        break;
    case INSTRUCTION_VA_COPY:
        ir_dump_va_copy_instruction(file, (ir_instr_va_copy_t*) instruction);
        break;
    case INSTRUCTION_ASM:
        fprintf(file, "inlineasm\n");
        break;
    case INSTRUCTION_DEINIT_SVARS:
        fprintf(file, "deinit_svars\n");
        break;
    case INSTRUCTION_UNREACHABLE:
        fprintf(file, "unreachable\n");
        break;
    default:
        printf("Unknown instruction id 0x%08X when dumping ir module\n", (int) instruction->id);
        fprintf(file, "<unknown instruction>\n");
    }
}
