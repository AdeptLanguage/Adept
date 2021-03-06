
#include "IR/ir.h"
#include "IR/ir_lowering.h"
#include "IRGEN/ir_builder.h"
#include "UTIL/util.h"
#include "UTIL/color.h"
#include "UTIL/datatypes.h"

strong_cstr_t ir_value_str(ir_value_t *value){
    if(value == NULL){
        internalerrorprintf("The value passed to ir_value_str is NULL, a crash will probably follow...\n");
    }

    if(value->type == NULL){
        internalerrorprintf("The value passed to ir_value_str has a type pointer to NULL, a crash will probably follow...\n");
    }

    char *typename = ir_type_str(value->type);
    length_t typename_length = strlen(typename);
    char *value_str;

    switch(value->value_type){
    case VALUE_TYPE_LITERAL:
        switch(value->type->kind){
        case TYPE_KIND_S8:
            value_str = malloc(typename_length + 22);
            sprintf(value_str, "%s %"PRId8, typename, *((adept_byte*) value->extra));
            free(typename);
            return value_str;
        case TYPE_KIND_U8:
            value_str = malloc(typename_length + 22);
            sprintf(value_str, "%s %"PRIu8, typename, *((adept_ubyte*) value->extra));
            free(typename);
            return value_str;
        case TYPE_KIND_S16:
            value_str = malloc(typename_length + 22);
            sprintf(value_str, "%s %"PRId16, typename, *((adept_short*) value->extra));
            free(typename);
            return value_str;
        case TYPE_KIND_U16:
            value_str = malloc(typename_length + 22);
            sprintf(value_str, "%s %"PRIu16, typename, *((adept_ushort*) value->extra));
            free(typename);
            return value_str;
        case TYPE_KIND_S32:
            value_str = malloc(typename_length + 22);
            sprintf(value_str, "%s %"PRId32, typename, *((adept_int*) value->extra));
            free(typename);
            return value_str;
        case TYPE_KIND_U32:
            value_str = malloc(typename_length + 22);
            sprintf(value_str, "%s %"PRIu32, typename, *((adept_uint*) value->extra));
            free(typename);
            return value_str;
        case TYPE_KIND_S64:
            value_str = malloc(typename_length + 22);
            sprintf(value_str, "%s %"PRId64, typename, *((adept_long*) value->extra));
            free(typename);
            return value_str;
        case TYPE_KIND_U64:
            value_str = malloc(typename_length + 22);
            sprintf(value_str, "%s %"PRIu64, typename, *((adept_ulong*) value->extra));
            free(typename);
            return value_str;
        case TYPE_KIND_FLOAT:
            value_str = malloc(typename_length + 21);
            sprintf(value_str, "%s %06.6f", typename, (double) *((adept_float*) value->extra));
            free(typename);
            return value_str;
        case TYPE_KIND_DOUBLE:
            value_str = malloc(typename_length + 21);
            sprintf(value_str, "%s %06.6f", typename, (double) *((adept_double*) value->extra));
            free(typename);
            return value_str;
        case TYPE_KIND_BOOLEAN:
            value_str = malloc(typename_length + 7);
            sprintf(value_str, "%s %s", typename, *((adept_bool*) value->extra) ? "true" : "false");
            free(typename);
            return value_str;
        case TYPE_KIND_POINTER:
            if( ((ir_type_t*) value->type->extra)->kind == TYPE_KIND_U8 ){
                // Null-terminated string literal
                char *string_data = (char*) value->extra;
                length_t string_data_length = strlen(string_data);
                length_t put_index = 1;
                length_t special_characters = 0;

                for(length_t s = 0; s != string_data_length; s++){
                    if(string_data[s] <= 0x1F || string_data[s] == '\\' || string_data[s] == '\''){
                        special_characters++;
                    }
                }

                value_str = malloc(string_data_length + special_characters + 3);
                value_str[0] = '\'';
                value_str[string_data_length + special_characters + 1] = '\'';
                value_str[string_data_length + special_characters + 2] = '\0';

                for(length_t c = 0; c != string_data_length; c++){
                    if((string_data[c] > 0x1F || string_data[c] == '\\') && string_data[c] != '\'')value_str[put_index++] = string_data[c];
                    else {
                        switch(string_data[c]){
                        case '\n':
                            value_str[put_index++] = '\\';
                            value_str[put_index++] = 'n';
                            break;
                        case '\r':
                            value_str[put_index++] = '\\';
                            value_str[put_index++] = 'r';
                            break;
                        case '\t':
                            value_str[put_index++] = '\\';
                            value_str[put_index++] = 't';
                            break;
                        case '\\':
                            value_str[put_index++] = '\\';
                            value_str[put_index++] = '\\';
                            break;
                        case '\'':
                            value_str[put_index++] = '\\';
                            value_str[put_index++] = '\'';
                            break;
                        default:
                            value_str[put_index++] = '\\';
                            value_str[put_index++] = '?';
                            break;
                        }
                    }
                }

                free(typename);
                return value_str;
            }
            break; // Break to unrecognized type kind
        }
        free(typename);
        printf("INTERNAL ERROR: Unrecognized type kind %d in ir_value_str\n", (int) value->value_type);
        return NULL;
    case VALUE_TYPE_RESULT:
        // ">|-0000000000| 0x00000000<"
        value_str = malloc(typename_length + 28); // 28 is a safe number I think
        sprintf(value_str, "%s >|%d| 0x%08X<", typename, (int) ((ir_value_result_t*) value->extra)->block_id, (int) ((ir_value_result_t*) value->extra)->instruction_id);
        free(typename);
        return value_str;
    case VALUE_TYPE_NULLPTR: case VALUE_TYPE_NULLPTR_OF_TYPE:
        value_str = malloc(5);
        memcpy(value_str, "null", 5);
        free(typename);
        return value_str;
    case VALUE_TYPE_ARRAY_LITERAL:
        value_str = malloc(5);
        memcpy(value_str, "larr", 5);
        free(typename); 
        return value_str;
    case VALUE_TYPE_STRUCT_LITERAL:
        value_str = malloc(5);
        memcpy(value_str, "stru", 5);
        free(typename);
        return value_str;
    case VALUE_TYPE_ANON_GLOBAL:
        value_str = malloc(32);
        sprintf(value_str, "anonglob %d", (int) ((ir_value_anon_global_t*) value->extra)->anon_global_id);
        free(typename);
        return value_str;
    case VALUE_TYPE_CONST_ANON_GLOBAL:
        value_str = malloc(32);
        sprintf(value_str, "constanonglob %d", (int) ((ir_value_anon_global_t*) value->extra)->anon_global_id);
        free(typename);
        return value_str;
    case VALUE_TYPE_CSTR_OF_LEN: {
            ir_value_cstr_of_len_t *cstroflen = value->extra;
            value_str = malloc(32 + cstroflen->length);
            sprintf(value_str, "cstroflen %d \"%s\"", (int) cstroflen->length, cstroflen->array);
            free(typename);
            return value_str;
        }
        break;
    case VALUE_TYPE_CONST_SIZEOF: {
            ir_value_const_sizeof_t *const_sizeof = value->extra;
            char *type_str = ir_type_str(const_sizeof->type);
            value_str = mallocandsprintf("csizeof %s", type_str);
            free(type_str);
            free(typename);
            return value_str;
        }
        break;
    case VALUE_TYPE_CONST_ADD: {
            ir_value_const_math_t *const_add = value->extra;
            char *a_str = ir_value_str(const_add->a);
            char *b_str = ir_value_str(const_add->b);
            value_str = mallocandsprintf("cadd %s, %s", a_str, b_str);
            free(a_str);
            free(b_str);
            free(typename);
            return value_str;
        }
        break;
    case VALUE_TYPE_CONST_BITCAST:
    case VALUE_TYPE_CONST_ZEXT:
    case VALUE_TYPE_CONST_SEXT:
    case VALUE_TYPE_CONST_FEXT:
    case VALUE_TYPE_CONST_TRUNC:
    case VALUE_TYPE_CONST_FTRUNC:
    case VALUE_TYPE_CONST_INTTOPTR:
    case VALUE_TYPE_CONST_PTRTOINT:
    case VALUE_TYPE_CONST_FPTOUI:
    case VALUE_TYPE_CONST_FPTOSI:
    case VALUE_TYPE_CONST_UITOFP:
    case VALUE_TYPE_CONST_SITOFP:
    case VALUE_TYPE_CONST_REINTERPRET: {
            weak_cstr_t const_cast_name = "<unknown const cast>";

            switch(value->value_type){
            case VALUE_TYPE_CONST_BITCAST:     const_cast_name = "cbc";     break;
            case VALUE_TYPE_CONST_ZEXT:        const_cast_name = "czext";   break;
            case VALUE_TYPE_CONST_SEXT:        const_cast_name = "csext";   break;
            case VALUE_TYPE_CONST_FEXT:        const_cast_name = "cfext";   break;
            case VALUE_TYPE_CONST_TRUNC:       const_cast_name = "ctrunc";  break;
            case VALUE_TYPE_CONST_FTRUNC:      const_cast_name = "cftrunc"; break;
            case VALUE_TYPE_CONST_INTTOPTR:    const_cast_name = "ci2p";    break;
            case VALUE_TYPE_CONST_PTRTOINT:    const_cast_name = "cp2i";    break;
            case VALUE_TYPE_CONST_FPTOUI:      const_cast_name = "cfp2ui";  break;
            case VALUE_TYPE_CONST_FPTOSI:      const_cast_name = "cfp2si";  break;
            case VALUE_TYPE_CONST_UITOFP:      const_cast_name = "cui2fp";  break;
            case VALUE_TYPE_CONST_SITOFP:      const_cast_name = "csi2fp";  break;
            case VALUE_TYPE_CONST_REINTERPRET: const_cast_name = "creinterp";  break;
            }

            strong_cstr_t from = ir_value_str(value->extra);
            strong_cstr_t to = ir_type_str(value->type);
            value_str = malloc(32 + strlen(to) + strlen(from));
            sprintf(value_str, "%s %s to %s", const_cast_name, from, to);
            free(to);
            free(from);
            free(typename);
            return value_str;
        }
        break;
    case VALUE_TYPE_STRUCT_CONSTRUCTION: {
            ir_value_struct_construction_t *construction = (ir_value_struct_construction_t*) value->extra;
            strong_cstr_t of = ir_type_str(value->type);
            value_str = malloc(40 + strlen(of));
            sprintf(value_str, "construct %s (from %d values)", of, (int) construction->length);
            free(of);
            free(typename);
            return value_str;
        }
        break;
    default:
        internalerrorprintf("Unexpected value type of value in ir_value_str function\n");
        free(typename);
        return NULL;
    }

    free(typename);
    return NULL; // Should never get here
}

successful_t ir_type_map_find(ir_type_map_t *type_map, char *name, ir_type_t **type_ptr){
    // Does a binary search on the type map to find the requested type by name

    ir_type_mapping_t *mappings = type_map->mappings;
    length_t mappings_length = type_map->mappings_length;
    int first = 0, middle, last = mappings_length - 1, comparison;

    while(first <= last){
        middle = (first + last) / 2;
        comparison = strcmp(mappings[middle].name, name);

        if(comparison == 0){
            *type_ptr = &mappings[middle].type;
            return true;
        }
        else if(comparison > 0) last = middle - 1;
        else first = middle + 1;
    }

    return false;
}

void ir_basicblock_new_instructions(ir_basicblock_t *block, length_t amount){
    // NOTE: Ensures that there is enough room for 'amount' more instructions
    // NOTE: If there isn't, more memory will be allocated so they can be generated
    if(block->instructions_length + amount >= block->instructions_capacity){
        ir_instr_t **new_instructions = malloc(sizeof(ir_instr_t*) * block->instructions_capacity * 2);
        memcpy(new_instructions, block->instructions, sizeof(ir_instr_t*) * block->instructions_length);
        block->instructions_capacity *= 2;
        free(block->instructions);
        block->instructions = new_instructions;
    }
}

void ir_module_dump(ir_module_t *ir_module, const char *filename){
    // Dumps an ir_module_t to a file
    FILE *file = fopen(filename, "w");
    if(file == NULL){
        printf("INTERNAL ERROR: Failed to open ir module dump file\n");
        return;
    }
    ir_dump_functions(file, ir_module->funcs, ir_module->funcs_length);
    fclose(file);
}

void ir_dump_functions(FILE *file, ir_func_t *functions, length_t functions_length){
    // NOTE: Dumps the functions in an ir_module_t to a file
    // TODO: Clean up this function

    char implementation_buffer[32];

    for(length_t f = 0; f != functions_length; f++){
        // Print prototype
        char *ret_type_str = ir_type_str(functions[f].return_type);
        ir_implementation(f, 'a', implementation_buffer);
        fprintf(file, "fn %s %s -> %s\n", functions[f].name, implementation_buffer, ret_type_str);
        free(ret_type_str);

        // Count the total number of instructions
        length_t total_instructions = 0;
        for(length_t b = 0; b != functions[f].basicblocks_length; b++) total_instructions += functions[f].basicblocks[b].instructions_length;

        // Print functions statistics
        if(!(functions[f].traits & IR_FUNC_FOREIGN))
            fprintf(file, "    {%d BBs, %d INSTRs, %d VARs}\n", (int) functions[f].basicblocks_length, (int) total_instructions, (int) functions[f].variable_count);

        ir_dump_var_scope_layout(file, functions[f].scope);
        ir_dump_basicsblocks(file, functions[f].basicblocks, functions[f].basicblocks_length, functions);
    }
}

void ir_dump_basicsblocks(FILE *file, ir_basicblock_t *basicblocks, length_t basicblocks_length, ir_func_t *functions){
    char *val_str, *dest_str, *idx_str;

    for(length_t b = 0; b != basicblocks_length; b++){
        fprintf(file, "  BASICBLOCK |%d|\n", (int) b);
        for(length_t i = 0; i != basicblocks[b].instructions_length; i++){
            switch(basicblocks[b].instructions[i]->id){
            case INSTRUCTION_RET: {
                    ir_value_t *val = ((ir_instr_ret_t*) basicblocks[b].instructions[i])->value;
                    if(val != NULL){
                        val_str = ir_value_str(val);
                        fprintf(file, "    0x%08X ret %s\n", (int) i, val_str);
                        free(val_str);
                    } else {
                        fprintf(file, "    0x%08X ret\n", (int) i);
                    }
                    break;
                }
            case INSTRUCTION_ADD:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "add");
                break;
            case INSTRUCTION_FADD:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "fadd");
                break;
            case INSTRUCTION_SUBTRACT:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "sub");
                break;
            case INSTRUCTION_FSUBTRACT:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "fsub");
                break;
            case INSTRUCTION_MULTIPLY:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "mul");
                break;
            case INSTRUCTION_FMULTIPLY:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "fmul");
                break;
            case INSTRUCTION_UDIVIDE:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "udiv");
                break;
            case INSTRUCTION_SDIVIDE:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "sdiv");
                break;
            case INSTRUCTION_FDIVIDE:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "fdiv");
                break;
            case INSTRUCTION_UMODULUS:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "urem");
                break;
            case INSTRUCTION_SMODULUS:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "srem");
                break;
            case INSTRUCTION_FMODULUS:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "frem");
                break;
            case INSTRUCTION_CALL: {
                    const char *real_name = functions[((ir_instr_call_t*) basicblocks[b].instructions[i])->ir_func_id].name;
                    ir_dump_call_instruction(file, (ir_instr_call_t*) basicblocks[b].instructions[i], i, real_name);
                }
                break;
            case INSTRUCTION_CALL_ADDRESS:
                ir_dump_call_address_instruction(file, (ir_instr_call_address_t*) basicblocks[b].instructions[i], i);
                break;
            case INSTRUCTION_MALLOC: {
                    ir_instr_malloc_t *malloc_instr = (ir_instr_malloc_t*) basicblocks[b].instructions[i];
                    char *typename = ir_type_str(malloc_instr->type);
                    if(malloc_instr->amount == NULL){
                        fprintf(file, "    0x%08X malloc %s\n", (int) i, typename);
                    } else {
                        char *a = ir_value_str(malloc_instr->amount);
                        fprintf(file, "    0x%08X malloc %s * %s\n", (int) i, typename, a);
                        free(a);
                    }
                    free(typename);
                }
                break;
            case INSTRUCTION_FREE: {
                    val_str = ir_value_str(((ir_instr_free_t*) basicblocks[b].instructions[i])->value);
                    fprintf(file, "    0x%08X free %s\n", (int) i, val_str);
                    free(val_str);
                }
                break;
            case INSTRUCTION_STORE:
                val_str = ir_value_str(((ir_instr_store_t*) basicblocks[b].instructions[i])->value);
                dest_str = ir_value_str(((ir_instr_store_t*) basicblocks[b].instructions[i])->destination);
                fprintf(file, "    0x%08X store %s, %s\n", (int) i, dest_str, val_str);
                free(val_str);
                free(dest_str);
                break;
            case INSTRUCTION_LOAD:
                val_str = ir_value_str(((ir_instr_load_t*) basicblocks[b].instructions[i])->value);
                fprintf(file, "    0x%08X load %s\n", (int) i, val_str);
                free(val_str);
                break;
            case INSTRUCTION_VARPTR: {
                    char *var_type = ir_type_str(basicblocks[b].instructions[i]->result_type);
                    fprintf(file, "    0x%08X var %s 0x%08X\n", (int) i, var_type, (int) ((ir_instr_varptr_t*) basicblocks[b].instructions[i])->index);
                    free(var_type);
                }
                break;
            case INSTRUCTION_GLOBALVARPTR: {
                    char *var_type = ir_type_str(basicblocks[b].instructions[i]->result_type);
                    fprintf(file, "    0x%08X gvar %s 0x%08X\n", (int) i, var_type, (int) ((ir_instr_varptr_t*) basicblocks[b].instructions[i])->index);
                    free(var_type);
                }
                break;
            case INSTRUCTION_STATICVARPTR: {
                    char *var_type = ir_type_str(basicblocks[b].instructions[i]->result_type);
                    fprintf(file, "    0x%08X svar %s 0x%08X\n", (int) i, var_type, (int) ((ir_instr_varptr_t*) basicblocks[b].instructions[i])->index);
                    free(var_type);
                }
                break;
            case INSTRUCTION_BREAK:
                fprintf(file, "    0x%08X br |%d|\n", (int) i, (int) ((ir_instr_break_t*) basicblocks[b].instructions[i])->block_id);
                break;
            case INSTRUCTION_CONDBREAK:
                val_str = ir_value_str(((ir_instr_cond_break_t*) basicblocks[b].instructions[i])->value);
                fprintf(file, "    0x%08X cbr %s, |%d|, |%d|\n", (int) i, val_str, (int) ((ir_instr_cond_break_t*) basicblocks[b].instructions[i])->true_block_id,
                    (int) ((ir_instr_cond_break_t*) basicblocks[b].instructions[i])->false_block_id);
                free(val_str);
                break;
            case INSTRUCTION_EQUALS:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "eq");
                break;
            case INSTRUCTION_FEQUALS:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "feq");
                break;
            case INSTRUCTION_NOTEQUALS:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "neq");
                break;
            case INSTRUCTION_FNOTEQUALS:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "fneq");
                break;
            case INSTRUCTION_UGREATER:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "ugt");
                break;
            case INSTRUCTION_SGREATER:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "sgt");
                break;
            case INSTRUCTION_FGREATER:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "fgt");
                break;
            case INSTRUCTION_ULESSER:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "ult");
                break;
            case INSTRUCTION_SLESSER:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "slt");
                break;
            case INSTRUCTION_FLESSER:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "flt");
                break;
            case INSTRUCTION_UGREATEREQ:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "uge");
                break;
            case INSTRUCTION_SGREATEREQ:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "sge");
                break;
            case INSTRUCTION_FGREATEREQ:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "fge");
                break;
            case INSTRUCTION_ULESSEREQ:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "ule");
                break;
            case INSTRUCTION_SLESSEREQ:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "sle");
                break;
            case INSTRUCTION_FLESSEREQ:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "fle");
                break;
            case INSTRUCTION_MEMBER:
                val_str = ir_value_str(((ir_instr_member_t*) basicblocks[b].instructions[i])->value);
                fprintf(file, "    0x%08X memb %s, %d\n", (unsigned int) i, val_str, (int) ((ir_instr_member_t*) basicblocks[b].instructions[i])->member);
                free(val_str);
                break;
            case INSTRUCTION_ARRAY_ACCESS:
                val_str = ir_value_str(((ir_instr_array_access_t*) basicblocks[b].instructions[i])->value);
                idx_str = ir_value_str(((ir_instr_array_access_t*) basicblocks[b].instructions[i])->index);
                fprintf(file, "    0x%08X arracc %s, %s\n", (unsigned int) i, val_str, idx_str);
                free(val_str);
                free(idx_str);
                break;
            case INSTRUCTION_FUNC_ADDRESS:
                if(((ir_instr_func_address_t*) basicblocks[b].instructions[i])->name == NULL){
                    char buffer[32];
                    ir_implementation(((ir_instr_func_address_t*) basicblocks[b].instructions[i])->ir_func_id, 0x00, buffer);
                    fprintf(file, "    0x%08X funcaddr 0x%s\n", (unsigned int) i, buffer);
                } else
                    fprintf(file, "    0x%08X funcaddr %s\n", (unsigned int) i, ((ir_instr_func_address_t*) basicblocks[b].instructions[i])->name);
                break;
            case INSTRUCTION_BITCAST: case INSTRUCTION_ZEXT: case INSTRUCTION_SEXT:
            case INSTRUCTION_FEXT: case INSTRUCTION_TRUNC: case INSTRUCTION_FTRUNC:
            case INSTRUCTION_INTTOPTR: case INSTRUCTION_PTRTOINT: case INSTRUCTION_FPTOUI:
            case INSTRUCTION_FPTOSI: case INSTRUCTION_UITOFP: case INSTRUCTION_SITOFP:
            case INSTRUCTION_REINTERPRET: {
                    char *instr_name = "";

                    switch(basicblocks[b].instructions[i]->id){
                    case INSTRUCTION_BITCAST:     instr_name = "bc";     break;
                    case INSTRUCTION_ZEXT:        instr_name = "zext";   break;
                    case INSTRUCTION_SEXT:        instr_name = "sext";   break;
                    case INSTRUCTION_FEXT:        instr_name = "fext";   break;
                    case INSTRUCTION_TRUNC:       instr_name = "trunc";  break;
                    case INSTRUCTION_FTRUNC:      instr_name = "ftrunc"; break;
                    case INSTRUCTION_INTTOPTR:    instr_name = "i2p";    break;
                    case INSTRUCTION_PTRTOINT:    instr_name = "p2i";    break;
                    case INSTRUCTION_FPTOUI:      instr_name = "fp2ui";  break;
                    case INSTRUCTION_FPTOSI:      instr_name = "fp2si";  break;
                    case INSTRUCTION_UITOFP:      instr_name = "ui2fp";  break;
                    case INSTRUCTION_SITOFP:      instr_name = "si2fp";  break;
                    case INSTRUCTION_REINTERPRET: instr_name = "reinterp";  break;
                    }

                    char *to_type = ir_type_str(basicblocks[b].instructions[i]->result_type);
                    val_str = ir_value_str(((ir_instr_cast_t*) basicblocks[b].instructions[i])->value);
                    fprintf(file, "    0x%08X %s %s to %s\n", (int) i, instr_name, val_str, to_type);
                    free(to_type);
                    free(val_str);
                }
                break;
            case INSTRUCTION_ISZERO: case INSTRUCTION_ISNTZERO: {
                    char *instr_name = basicblocks[b].instructions[i]->id == INSTRUCTION_ISZERO ? "isz" : "inz";
                    val_str = ir_value_str(((ir_instr_cast_t*) basicblocks[b].instructions[i])->value);
                    fprintf(file, "    0x%08X %s %s\n", (int) i, instr_name, val_str);
                    free(val_str);
                }
                break;
            case INSTRUCTION_AND:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "and");
                break;
            case INSTRUCTION_OR:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "or");
                break;
            case INSTRUCTION_SIZEOF: {
                    char *typename = ir_type_str(((ir_instr_sizeof_t*) basicblocks[b].instructions[i])->type);
                    fprintf(file, "    0x%08X sizeof %s\n", (int) i, typename);
                    free(typename);
                }
                break;
            case INSTRUCTION_OFFSETOF: {
                    char *typename = ir_type_str(((ir_instr_offsetof_t*) basicblocks[b].instructions[i])->type);
                    fprintf(file, "    0x%08X offsetof %s\n", (int) i, typename);
                    free(typename);
                }
                break;
            case INSTRUCTION_ZEROINIT:
                val_str = ir_value_str(((ir_instr_zeroinit_t*) basicblocks[b].instructions[i])->destination);
                fprintf(file, "    0x%08X zi %s\n", (int) i, val_str);
                free(val_str);
                break;
            case INSTRUCTION_MEMCPY: {
                    ir_instr_memcpy_t *memcpy_instr = (ir_instr_memcpy_t*) basicblocks[b].instructions[i];
                    strong_cstr_t destination = ir_value_str(memcpy_instr->destination);
                    strong_cstr_t value = ir_value_str(memcpy_instr->value);
                    strong_cstr_t bytes = ir_value_str(memcpy_instr->bytes);
                    fprintf(file, "    0x%08X memcpy %s, %s, %s\n", (int) i, destination, value, bytes);
                    free(destination);
                    free(value);
                    free(bytes);
                }
                break;
            case INSTRUCTION_BIT_AND:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "band");
                break;
            case INSTRUCTION_BIT_OR:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "bor");
                break;
            case INSTRUCTION_BIT_XOR:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "bxor");
                break;
            case INSTRUCTION_BIT_LSHIFT:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "lshift");
                break;
            case INSTRUCTION_BIT_RSHIFT:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "rshift");
                break;
            case INSTRUCTION_BIT_LGC_RSHIFT:
                ir_dump_math_instruction(file, (ir_instr_math_t*) basicblocks[b].instructions[i], i, "lgcrshift");
                break;
            case INSTRUCTION_BIT_COMPLEMENT:
                val_str = ir_value_str(((ir_instr_load_t*) basicblocks[b].instructions[i])->value);
                fprintf(file, "    0x%08X compl %s\n", (int) i, val_str);
                free(val_str);
                break;
            case INSTRUCTION_NEGATE:
                val_str = ir_value_str(((ir_instr_load_t*) basicblocks[b].instructions[i])->value);
                fprintf(file, "    0x%08X neg %s\n", (int) i, val_str);
                free(val_str);
                break;
            case INSTRUCTION_FNEGATE:
                val_str = ir_value_str(((ir_instr_load_t*) basicblocks[b].instructions[i])->value);
                fprintf(file, "    0x%08X fneg %s\n", (int) i, val_str);
                free(val_str);
                break;
            case INSTRUCTION_PHI2: {
                    ir_instr_phi2_t *phi2 = (ir_instr_phi2_t*) basicblocks[b].instructions[i];
                    char *a = ir_value_str(phi2->a);
                    char *b = ir_value_str(phi2->b);
                    fprintf(file, "    0x%08X phi2 |%d| -> %s, |%d| -> %s\n", (int) i, (int) phi2->block_id_a, a, (int) phi2->block_id_b, b);
                    free(a);
                    free(b);
                }
                break;
            case INSTRUCTION_SWITCH: {
                    ir_instr_switch_t *switch_instr = (ir_instr_switch_t*) basicblocks[b].instructions[i];
                    char *cond = ir_value_str(switch_instr->condition);
                    fprintf(file, "    0x%08X switch %s\n", (int) i, cond);
                    for(length_t i = 0; i != switch_instr->cases_length; i++){
                        char *v = ir_value_str(switch_instr->case_values[i]);
                        fprintf(file, "               (%s) -> |%d|\n", v, (int) switch_instr->case_block_ids[i]);
                        free(v);
                    }
                    free(cond);

                    if(switch_instr->default_block_id != switch_instr->resume_block_id){
                        // Default case exists
                        fprintf(file, "               default -> |%d|\n", (int) switch_instr->case_block_ids[i]);
                    }
                }
                break;
            case INSTRUCTION_ALLOC: {
                    ir_instr_alloc_t *alloc = (ir_instr_alloc_t*) basicblocks[b].instructions[i];
                    ir_type_t *alloc_result_type = alloc->result_type;
                    
                    if(alloc_result_type->kind != TYPE_KIND_POINTER){
                        fprintf(file, "    0x%08X alloc (malformed)\n", (int) i);
                    } else {
                        char *typename = ir_type_str(alloc_result_type->extra);
                        char *count = alloc->count ? ir_value_str(alloc->count) : strclone("single");
                        fprintf(file, "    0x%08X alloc %s, aligned %d, count %s\n", (int) i, typename, alloc->alignment, count);
                        free(count);
                        free(typename);
                    }
                }
                break;
            case INSTRUCTION_STACK_SAVE:
                fprintf(file, "    0x%08X ssave\n", (int) i);
                break;
            case INSTRUCTION_STACK_RESTORE:
                val_str = ir_value_str(((ir_instr_stack_restore_t*) basicblocks[b].instructions[i])->stack_pointer);
                fprintf(file, "    0x%08X srestore %s\n", (int) i, val_str);
                free(val_str);
                break;
            case INSTRUCTION_VA_START:
                val_str = ir_value_str(((ir_instr_unary_t*) basicblocks[b].instructions[i])->value);
                fprintf(file, "    0x%08X va_start %s\n", (int) i, val_str);
                free(val_str);
                break;
            case INSTRUCTION_VA_END:
                val_str = ir_value_str(((ir_instr_unary_t*) basicblocks[b].instructions[i])->value);
                fprintf(file, "    0x%08X va_end %s\n", (int) i, val_str);
                free(val_str);
                break;
            case INSTRUCTION_VA_ARG: {
                    val_str = ir_value_str(((ir_instr_va_arg_t*) basicblocks[b].instructions[i])->va_list);
                    strong_cstr_t type_str = ir_type_str(((ir_instr_va_arg_t*) basicblocks[b].instructions[i])->result_type);

                    fprintf(file, "    0x%08X va_arg %s, %s\n", (int) i, val_str, type_str);
                    
                    free(val_str);
                    free(type_str);
                }
                break;
            case INSTRUCTION_VA_COPY: {
                    strong_cstr_t dest_str = ir_value_str(((ir_instr_va_copy_t*) basicblocks[b].instructions[i])->dest_value);
                    val_str = ir_value_str(((ir_instr_va_copy_t*) basicblocks[b].instructions[i])->src_value);

                    fprintf(file, "    0x%08X va_copy %s, %s\n", (int) i, dest_str, val_str);
                    
                    free(val_str);
                    free(dest_str);
                }
                break;
            case INSTRUCTION_ASM: {
                    fprintf(file, "    0x%08X inlineasm\n", (int) i);
                }
                break;
            case INSTRUCTION_DEINIT_SVARS:
                fprintf(file, "    0x%08X deinit_svars\n", (int) i);
                break;
            default:
                printf("Unknown instruction id 0x%08X when dumping ir module\n", (int) basicblocks[b].instructions[i]->id);
                fprintf(file, "    0x%08X <unknown instruction>\n", (int) i);
            }
        }
    }
}

void ir_dump_math_instruction(FILE *file, ir_instr_math_t *instruction, int i, const char *instruction_name){
    char *val_str_1 = ir_value_str(instruction->a);
    char *val_str_2 = ir_value_str(instruction->b);
    fprintf(file, "    0x%08X %s %s, %s\n", i, instruction_name, val_str_1, val_str_2);
    free(val_str_1);
    free(val_str_2);
}

void ir_dump_call_instruction(FILE *file, ir_instr_call_t *instruction, int instr_i, const char *real_name){
    char *call_args = malloc(256);
    length_t call_args_length = 0;
    length_t call_args_capacity = 256;
    char *call_result_type = ir_type_str(instruction->result_type);
    call_args[0] = '\0';

    for(length_t i = 0; i != instruction->values_length; i++){
        char *arg = ir_value_str(instruction->values[i]);
        length_t arg_length = strlen(arg);
        length_t target_length = call_args_length + arg_length + (i + 1 == instruction->values_length ? 1 : 3);

        while(target_length >= call_args_capacity){ // Three is for "\0" + possible ", "
            call_args_capacity *= 2;
            char *new_call_args = malloc(call_args_capacity);
            memcpy(new_call_args, call_args, call_args_length);
            free(call_args);
            call_args = new_call_args;
        }

        memcpy(&call_args[call_args_length], arg, arg_length + 1);
        call_args_length += arg_length;
        if(i + 1 != instruction->values_length) memcpy(&call_args[call_args_length], ", ", 2);
        call_args_length += 2;
        free(arg);
    }

    char implementation_buffer[32];
    ir_implementation((int) instruction->ir_func_id, 'a', implementation_buffer);
    fprintf(file, "    0x%08X call %s \"%s\" (%s) %s\n", instr_i, implementation_buffer, real_name, call_args, call_result_type);
    free(call_args);
    free(call_result_type);
}

void ir_dump_call_address_instruction(FILE *file, ir_instr_call_address_t *instruction, int i){
    char *call_args = malloc(256);
    length_t call_args_length = 0;
    length_t call_args_capacity = 256;
    char *call_result_type = ir_type_str(instruction->result_type);
    call_args[0] = '\0';

    for(length_t i = 0; i != instruction->values_length; i++){
        char *arg = ir_value_str(instruction->values[i]);
        length_t arg_length = strlen(arg);
        length_t target_length = call_args_length + arg_length + (i + 1 == instruction->values_length ? 1 : 3);

        while(target_length >= call_args_capacity){ // Three is for "\0" + possible ", "
            call_args_capacity *= 2;
            char *new_call_args = malloc(call_args_capacity);
            memcpy(new_call_args, call_args, call_args_length);
            free(call_args);
            call_args = new_call_args;
        }

        memcpy(&call_args[call_args_length], arg, arg_length + 1);
        call_args_length += arg_length;
        if(i + 1 != instruction->values_length) memcpy(&call_args[call_args_length], ", ", 2);
        call_args_length += 2;
        free(arg);
    }

    char *call_address = ir_value_str(instruction->address);
    fprintf(file, "    0x%08X calladdr %s(%s) %s\n", i, call_address, call_args, call_result_type);
    free(call_address);
    free(call_args);
    free(call_result_type);
}

void ir_dump_var_scope_layout(FILE *file, bridge_scope_t *scope){
    if(!scope) return;

    for(length_t v = 0; v != scope->list.length; v++){
        char *type_str = ir_type_str(scope->list.variables[v].ir_type);
        fprintf(file, "    [0x%08X %s]\n", (int) v, type_str);
        free(type_str);
    }

    for(length_t c = 0; c != scope->children_length; c++){
        ir_dump_var_scope_layout(file, scope->children[c]);
    }
}

void ir_module_init(ir_module_t *ir_module, length_t funcs_capacity, length_t globals_length, length_t func_mappings_length_guess){
    ir_pool_init(&ir_module->pool);
    
    ir_module->funcs = malloc(sizeof(ir_func_t) * funcs_capacity);
    ir_module->funcs_length = 0;
    ir_module->funcs_capacity = funcs_capacity;
    ir_module->func_mappings = malloc(sizeof(ir_func_mapping_t) * func_mappings_length_guess);
    ir_module->func_mappings_length = 0;
    ir_module->func_mappings_capacity = func_mappings_length_guess;
    ir_module->methods = NULL;
    ir_module->methods_length = 0;
    ir_module->methods_capacity = 0;
    ir_module->generic_base_methods = NULL;
    ir_module->generic_base_methods_length = 0;
    ir_module->generic_base_methods_capacity = 0;
    ir_module->type_map.mappings = NULL;
    ir_module->globals = malloc(sizeof(ir_global_t) * globals_length);
    ir_module->globals_length = 0;
    ir_module->anon_globals = NULL;
    ir_module->anon_globals_length = 0;
    ir_module->anon_globals_capacity = 0;
    ir_gen_sf_cache_init(&ir_module->sf_cache);
    ir_module->rtti_relocations = NULL;
    ir_module->rtti_relocations_length = 0;
    ir_module->rtti_relocations_capacity = 0;
    ir_module->init_builder = NULL;
    ir_module->deinit_builder = NULL;
    ir_module->static_variables = NULL;
    ir_module->static_variables_length = 0;
    ir_module->static_variables_capacity = 0;

    // Initialize common data
    ir_shared_common_t *common = &ir_module->common;
    common->ir_ubyte = ir_pool_alloc(&ir_module->pool, sizeof(ir_type_t));
    common->ir_ubyte->kind = TYPE_KIND_U8;
    common->ir_usize = ir_pool_alloc(&ir_module->pool, sizeof(ir_type_t));
    common->ir_usize->kind = TYPE_KIND_U64;
    common->ir_usize_ptr = ir_type_pointer_to(&ir_module->pool, common->ir_usize);
    common->ir_ptr = ir_type_pointer_to(&ir_module->pool, common->ir_ubyte);
    common->ir_bool = ir_pool_alloc(&ir_module->pool, sizeof(ir_type_t));
    common->ir_bool->kind = TYPE_KIND_BOOLEAN;
    common->rtti_array_index = 0;
    common->has_rtti_array = TROOLEAN_UNKNOWN;
    common->has_main = false;
    common->ast_main_id = 0;
    common->ir_main_id = 0;
    common->next_static_variable_id = 0;
}

void ir_module_free(ir_module_t *ir_module){
    ir_module_free_funcs(ir_module->funcs, ir_module->funcs_length);
    free(ir_module->funcs);
    free(ir_module->func_mappings);
    free(ir_module->methods);
    free(ir_module->generic_base_methods);
    free(ir_module->type_map.mappings);
    free(ir_module->globals);
    free(ir_module->anon_globals);
    ir_pool_free(&ir_module->pool);
    ir_gen_sf_cache_free(&ir_module->sf_cache);

    // Free init_builder
    // TODO: Maybe refactor this code
    if(ir_module->init_builder){
        for(length_t i = 0; i < ir_module->init_builder->basicblocks_length; i++){
            ir_basicblock_free(&ir_module->init_builder->basicblocks[i]);
        }
        free(ir_module->init_builder);
    }

    // Free deinit_builder
    if(ir_module->deinit_builder){
        for(length_t i = 0; i < ir_module->deinit_builder->basicblocks_length; i++){
            ir_basicblock_free(&ir_module->deinit_builder->basicblocks[i]);
        }
        free(ir_module->deinit_builder);
    }


    free(ir_module->static_variables);
    
    for(length_t i = 0; i != ir_module->rtti_relocations_length; i++){
        free(ir_module->rtti_relocations[i].human_notation);
    }
    free(ir_module->rtti_relocations);
}

void ir_module_free_funcs(ir_func_t *funcs, length_t funcs_length){
    for(length_t f = 0; f != funcs_length; f++){
        for(length_t b = 0; b != funcs[f].basicblocks_length; b++){
            ir_basicblock_free(&funcs[f].basicblocks[b]);
        }
        free(funcs[f].basicblocks);
        free(funcs[f].argument_types);

        if(funcs[f].scope != NULL){
            bridge_scope_free(funcs[f].scope);
            free(funcs[f].scope);
        }
    }
}

void ir_basicblock_free(ir_basicblock_t *basicblock){
    free(basicblock->instructions);
}

char *ir_implementation_encoding = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
length_t ir_implementation_encoding_base;

void ir_implementation_setup(){
    ir_implementation_encoding_base = strlen(ir_implementation_encoding);
}

void ir_implementation(length_t id, char prefix, char *output_buffer){
    // NOTE: output_buffer is assumed to be able to hold 32 characters
    length_t length = 0;

    if(prefix != 0x00){
        output_buffer[length++] = prefix;
    }
    
    do {
        length_t digit = id % ir_implementation_encoding_base;
        output_buffer[length++] = ir_implementation_encoding[digit];
        id /= ir_implementation_encoding_base;

        if(length == 31){
            printf("INTERNAL ERROR: ir_implementation received too large of an id\n");
            break;
        }
    } while(id != 0);

    output_buffer[length] = 0x00;
}

unsigned long long ir_value_uniqueness_value(ir_pool_t *pool, ir_value_t **value){
    if(ir_lower_const_cast(pool, value) || (*value)->value_type != VALUE_TYPE_LITERAL){
        printf("INTERNAL ERROR: ir_value_uniqueness_value received a value that isn't a constant literal\n");
        return 0xFFFFFFFF;
    };
    
    switch((*value)->type->kind){
    case TYPE_KIND_BOOLEAN: return *((adept_bool*) (*value)->extra);
    case TYPE_KIND_S8:      return *((adept_byte*) (*value)->extra);
    case TYPE_KIND_U8:      return *((adept_ubyte*) (*value)->extra);
    case TYPE_KIND_S16:     return *((adept_short*) (*value)->extra);
    case TYPE_KIND_U16:     return *((adept_ushort*) (*value)->extra);
    case TYPE_KIND_S32:     return *((adept_int*) (*value)->extra);
    case TYPE_KIND_U32:     return *((adept_uint*) (*value)->extra);
    case TYPE_KIND_S64:     return *((adept_long*) (*value)->extra);
    case TYPE_KIND_U64:     return *((adept_ulong*) (*value)->extra);
    default:
        printf("INTERNAL ERROR: ir_value_uniqueness_value received a value that isn't a constant literal\n");
        return 0xFFFFFFFF;
    }
}

void ir_print_value(ir_value_t *value){
    char *s = ir_value_str(value);
    printf("%s\n", s);
    free(s);
}

void ir_print_type(ir_type_t *type){
    char *s = ir_type_str(type);
    printf("%s\n", s);
    free(s);
}

void ir_module_insert_method(ir_module_t *module, weak_cstr_t struct_name, weak_cstr_t method_name, length_t ir_func_id, length_t ast_func_id, bool preserve_sortedness){
    ir_method_t method;
    method.struct_name = struct_name;
    method.name = method_name;
    method.ir_func_id = ir_func_id;
    method.ast_func_id = ast_func_id;
    method.is_beginning_of_group = -1;

    // Make room for the additional method
    expand((void**) &module->methods, sizeof(ir_method_t), module->methods_length, &module->methods_capacity, 1, 4);

    if(preserve_sortedness){
        // Find where to insert into the method list
        length_t insert_position = ir_module_find_insert_method_position(module, &method);
        
        // Move other methods over
        memmove(&module->methods[insert_position + 1], &module->methods[insert_position], sizeof(ir_method_t) * (module->methods_length - insert_position));

        // Invalidate whether method after is beginning of group
        if(insert_position != module->methods_length++)
            module->methods[insert_position + 1].is_beginning_of_group = -1;

        // New method available
        memcpy(&module->methods[insert_position], &method, sizeof(ir_method_t));
    } else {
        // New method available
        memcpy(&module->methods[module->methods_length++], &method, sizeof(ir_method_t));
    }
}

void ir_module_insert_generic_method(ir_module_t *module, 
    weak_cstr_t generic_base,
    ast_type_t *weak_generics,
    length_t generics_length,
    weak_cstr_t name,
    length_t ir_func_id,
    length_t ast_func_id,
bool preserve_sortedness){
    
    ir_generic_base_method_t method;
    method.generic_base = generic_base;
    method.generics = weak_generics;
    method.generics_length = generics_length;
    method.name = name;
    method.ir_func_id = ir_func_id;
    method.ast_func_id = ast_func_id;
    method.is_beginning_of_group = -1;

    // Make room for the additional method
    expand((void**) &module->generic_base_methods, sizeof(ir_generic_base_method_t), module->generic_base_methods_length, &module->generic_base_methods_capacity, 1, 4);

    if(preserve_sortedness){
        // Find where to insert into the method list
        length_t insert_position = ir_module_find_insert_generic_method_position(module, &method);
        
        // Move other methods over
        memmove(
            &module->generic_base_methods[insert_position + 1],
            &module->generic_base_methods[insert_position],
            sizeof(ir_generic_base_method_t) * (module->generic_base_methods_length - insert_position)    
        );

        // Invalidate whether method after is beginning of group
        if(insert_position != module->generic_base_methods_length++)
            module->generic_base_methods[insert_position + 1].is_beginning_of_group = -1;

        // New method available
        memcpy(&module->generic_base_methods[insert_position], &method, sizeof(ir_generic_base_method_t));
    } else {
        // New method available
        memcpy(&module->generic_base_methods[module->generic_base_methods_length++], &method, sizeof(ir_generic_base_method_t));
    }
}

ir_func_mapping_t *ir_module_insert_func_mapping(ir_module_t *module, weak_cstr_t name, length_t ir_func_id, length_t ast_func_id, bool preserve_sortedness){
    ir_func_mapping_t mapping, *result;
    mapping.name = name;
    mapping.ir_func_id = ir_func_id;
    mapping.ast_func_id = ast_func_id;
    mapping.is_beginning_of_group = -1;

    // Make room for the additional function mapping
    expand((void**) &module->func_mappings, sizeof(ir_func_mapping_t), module->func_mappings_length, &module->func_mappings_capacity, 1, 32);

    if(preserve_sortedness){
        // Find where to insert into the mappings list
        length_t insert_position = ir_module_find_insert_mapping_position(module, &mapping);
        
        // Move other mappings over
        memmove(
            &module->func_mappings[insert_position + 1],
            &module->func_mappings[insert_position],
            sizeof(ir_func_mapping_t) * (module->func_mappings_length - insert_position)    
        );
        
        // Invalidate whether mapping after is beginning of group
        if(insert_position != module->func_mappings_length++)
            module->func_mappings[insert_position + 1].is_beginning_of_group = -1;
        
        // New mapping available
        result = &module->func_mappings[insert_position];
    } else {
        // New mapping available
        result = &module->func_mappings[module->func_mappings_length++];
    }

    memcpy(result, &mapping, sizeof(ir_func_mapping_t));
    return result;
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
