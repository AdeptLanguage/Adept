
#include "IR/ir.h"
#include "UTIL/util.h"
#include "UTIL/color.h"

strong_cstr_t ir_value_str(ir_value_t *value){
    if(value == NULL){
        terminal_set_color(TERMINAL_COLOR_RED);
        printf("INTERNAL ERROR: The value passed to ir_value_str is NULL, a crash will probably follow...\n");
        terminal_set_color(TERMINAL_COLOR_DEFAULT);
    }

    if(value->type == NULL){
        terminal_set_color(TERMINAL_COLOR_RED);
        printf("INTERNAL ERROR: The value passed to ir_value_str has a type pointer to NULL, a crash will probably follow...\n");
        terminal_set_color(TERMINAL_COLOR_DEFAULT);
    }

    char *typename = ir_type_str(value->type);
    length_t typename_length = strlen(typename);
    char *value_str;

    switch(value->value_type){
    case VALUE_TYPE_LITERAL:
        switch(value->type->kind){
        case TYPE_KIND_S8:
            value_str = malloc(typename_length + 21);
            sprintf(value_str, "%s %d", typename, (int) *((char*) value->extra));
            free(typename);
            return value_str;
        case TYPE_KIND_U8:
            value_str = malloc(typename_length + 21);
            sprintf(value_str, "%s %d", typename, (int) *((unsigned char*) value->extra));
            free(typename);
            return value_str;
        case TYPE_KIND_S16:
            value_str = malloc(typename_length + 21);
            sprintf(value_str, "%s %d", typename, *((int*) value->extra));
            free(typename);
            return value_str;
        case TYPE_KIND_U16:
            value_str = malloc(typename_length + 21);
            sprintf(value_str, "%s %d", typename, (int) *((unsigned int*) value->extra));
            free(typename);
            return value_str;
        case TYPE_KIND_S32:
            value_str = malloc(typename_length + 21);
            sprintf(value_str, "%s %d", typename, (int) *((long long*) value->extra));
            free(typename);
            return value_str;
        case TYPE_KIND_U32:
            value_str = malloc(typename_length + 21);
            sprintf(value_str, "%s %d", typename, (int) *((unsigned long long*) value->extra));
            free(typename);
            return value_str;
        case TYPE_KIND_S64:
            value_str = malloc(typename_length + 21);
            sprintf(value_str, "%s %d", typename, (int) *((long long*) value->extra));
            free(typename);
            return value_str;
        case TYPE_KIND_U64:
            value_str = malloc(typename_length + 21);
            sprintf(value_str, "%s %d", typename, (int) *((unsigned long long*) value->extra));
            free(typename);
            return value_str;
        case TYPE_KIND_FLOAT:
            value_str = malloc(typename_length + 21);
            sprintf(value_str, "%s %06.6f", typename, *((double*) value->extra));
            free(typename);
            return value_str;
        case TYPE_KIND_DOUBLE:
            value_str = malloc(typename_length + 21);
            sprintf(value_str, "%s %06.6f", typename, *((double*) value->extra));
            free(typename);
            return value_str;
        case TYPE_KIND_BOOLEAN:
            value_str = malloc(typename_length + 6);
            sprintf(value_str, "%s %s", typename, *((bool*) value->extra) ? "true" : "false");
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
    case VALUE_TYPE_CONST_BITCAST: {
            strong_cstr_t from = ir_value_str(value->extra);
            strong_cstr_t to = ir_type_str(value->type);
            value_str = malloc(32 + strlen(to) + strlen(from));
            sprintf(value_str, "cbc %s to %s", from, to);
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
        redprintf("INTERNAL ERROR: Unexpected value type of value in ir_value_str function\n");
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

        char *val_str, *dest_str, *idx_str;

        for(length_t b = 0; b != functions[f].basicblocks_length; b++){
            fprintf(file, "  BASICBLOCK |%d|\n", (int) b);
            for(length_t i = 0; i != functions[f].basicblocks[b].instructions_length; i++){
                switch(functions[f].basicblocks[b].instructions[i]->id){
                case INSTRUCTION_RET: {
                        ir_value_t *val = ((ir_instr_ret_t*) functions[f].basicblocks[b].instructions[i])->value;
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
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "add");
                    break;
                case INSTRUCTION_FADD:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "fadd");
                    break;
                case INSTRUCTION_SUBTRACT:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "sub");
                    break;
                case INSTRUCTION_FSUBTRACT:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "fsub");
                    break;
                case INSTRUCTION_MULTIPLY:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "mul");
                    break;
                case INSTRUCTION_FMULTIPLY:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "fmul");
                    break;
                case INSTRUCTION_UDIVIDE:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "udiv");
                    break;
                case INSTRUCTION_SDIVIDE:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "sdiv");
                    break;
                case INSTRUCTION_FDIVIDE:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "fdiv");
                    break;
                case INSTRUCTION_UMODULUS:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "urem");
                    break;
                case INSTRUCTION_SMODULUS:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "srem");
                    break;
                case INSTRUCTION_FMODULUS:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "frem");
                    break;
                case INSTRUCTION_CALL: {
                        const char *real_name = functions[((ir_instr_call_t*) functions[f].basicblocks[b].instructions[i])->ir_func_id].name;
                        ir_dump_call_instruction(file, (ir_instr_call_t*) functions[f].basicblocks[b].instructions[i], i, real_name);
                    }
                    break;
                case INSTRUCTION_CALL_ADDRESS:
                    ir_dump_call_address_instruction(file, (ir_instr_call_address_t*) functions[f].basicblocks[b].instructions[i], i);
                    break;
                case INSTRUCTION_MALLOC: {
                        ir_instr_malloc_t *malloc_instr = (ir_instr_malloc_t*) functions[f].basicblocks[b].instructions[i];
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
                        val_str = ir_value_str(((ir_instr_free_t*) functions[f].basicblocks[b].instructions[i])->value);
                        fprintf(file, "    0x%08X free %s\n", (int) i, val_str);
                        free(val_str);
                    }
                    break;
                case INSTRUCTION_STORE:
                    val_str = ir_value_str(((ir_instr_store_t*) functions[f].basicblocks[b].instructions[i])->value);
                    dest_str = ir_value_str(((ir_instr_store_t*) functions[f].basicblocks[b].instructions[i])->destination);
                    fprintf(file, "    0x%08X store %s, %s\n", (int) i, dest_str, val_str);
                    free(val_str);
                    free(dest_str);
                    break;
                case INSTRUCTION_LOAD:
                    val_str = ir_value_str(((ir_instr_load_t*) functions[f].basicblocks[b].instructions[i])->value);
                    fprintf(file, "    0x%08X load %s\n", (int) i, val_str);
                    free(val_str);
                    break;
                case INSTRUCTION_VARPTR: {
                        char *var_type = ir_type_str(functions[f].basicblocks[b].instructions[i]->result_type);
                        fprintf(file, "    0x%08X var %s 0x%08X\n", (int) i, var_type, (int) ((ir_instr_varptr_t*) functions[f].basicblocks[b].instructions[i])->index);
                        free(var_type);
                    }
                    break;
                case INSTRUCTION_GLOBALVARPTR: {
                        char *var_type = ir_type_str(functions[f].basicblocks[b].instructions[i]->result_type);
                        fprintf(file, "    0x%08X gvar %s 0x%08X\n", (int) i, var_type, (int) ((ir_instr_varptr_t*) functions[f].basicblocks[b].instructions[i])->index);
                        free(var_type);
                    }
                    break;
                case INSTRUCTION_BREAK:
                    fprintf(file, "    0x%08X br |%d|\n", (int) i, (int) ((ir_instr_break_t*) functions[f].basicblocks[b].instructions[i])->block_id);
                    break;
                case INSTRUCTION_CONDBREAK:
                    val_str = ir_value_str(((ir_instr_cond_break_t*) functions[f].basicblocks[b].instructions[i])->value);
                    fprintf(file, "    0x%08X cbr %s, |%d|, |%d|\n", (int) i, val_str, (int) ((ir_instr_cond_break_t*) functions[f].basicblocks[b].instructions[i])->true_block_id,
                        (int) ((ir_instr_cond_break_t*) functions[f].basicblocks[b].instructions[i])->false_block_id);
                    free(val_str);
                    break;
                case INSTRUCTION_EQUALS:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "eq");
                    break;
                case INSTRUCTION_FEQUALS:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "feq");
                    break;
                case INSTRUCTION_NOTEQUALS:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "neq");
                    break;
                case INSTRUCTION_FNOTEQUALS:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "fneq");
                    break;
                case INSTRUCTION_UGREATER:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "ugt");
                    break;
                case INSTRUCTION_SGREATER:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "sgt");
                    break;
                case INSTRUCTION_FGREATER:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "fgt");
                    break;
                case INSTRUCTION_ULESSER:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "ult");
                    break;
                case INSTRUCTION_SLESSER:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "slt");
                    break;
                case INSTRUCTION_FLESSER:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "flt");
                    break;
                case INSTRUCTION_UGREATEREQ:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "uge");
                    break;
                case INSTRUCTION_SGREATEREQ:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "sge");
                    break;
                case INSTRUCTION_FGREATEREQ:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "fge");
                    break;
                case INSTRUCTION_ULESSEREQ:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "ule");
                    break;
                case INSTRUCTION_SLESSEREQ:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "sle");
                    break;
                case INSTRUCTION_FLESSEREQ:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "fle");
                    break;
                case INSTRUCTION_MEMBER:
                    val_str = ir_value_str(((ir_instr_member_t*) functions[f].basicblocks[b].instructions[i])->value);
                    fprintf(file, "    0x%08X memb %s, %d\n", (unsigned int) i, val_str, (int) ((ir_instr_member_t*) functions[f].basicblocks[b].instructions[i])->member);
                    free(val_str);
                    break;
                case INSTRUCTION_ARRAY_ACCESS:
                    val_str = ir_value_str(((ir_instr_array_access_t*) functions[f].basicblocks[b].instructions[i])->value);
                    idx_str = ir_value_str(((ir_instr_array_access_t*) functions[f].basicblocks[b].instructions[i])->index);
                    fprintf(file, "    0x%08X arracc %s, %s\n", (unsigned int) i, val_str, idx_str);
                    free(val_str);
                    free(idx_str);
                    break;
                case INSTRUCTION_FUNC_ADDRESS:
                    if(((ir_instr_func_address_t*) functions[f].basicblocks[b].instructions[i])->name == NULL){
                        char buffer[32];
                        ir_implementation(((ir_instr_func_address_t*) functions[f].basicblocks[b].instructions[i])->ir_func_id, 0x00, buffer);
                        fprintf(file, "    0x%08X funcaddr 0x%s\n", (unsigned int) i, buffer);
                    } else
                        fprintf(file, "    0x%08X funcaddr %s\n", (unsigned int) i, ((ir_instr_func_address_t*) functions[f].basicblocks[b].instructions[i])->name);
                    break;
                case INSTRUCTION_BITCAST: case INSTRUCTION_ZEXT:
                case INSTRUCTION_FEXT: case INSTRUCTION_TRUNC:
                case INSTRUCTION_FTRUNC: case INSTRUCTION_INTTOPTR:
                case INSTRUCTION_PTRTOINT: case INSTRUCTION_FPTOUI:
                case INSTRUCTION_FPTOSI: case INSTRUCTION_UITOFP:
                case INSTRUCTION_SITOFP: case INSTRUCTION_REINTERPRET: {
                        char *instr_name = "";

                        switch(functions[f].basicblocks[b].instructions[i]->id){
                        case INSTRUCTION_BITCAST:     instr_name = "bc";     break;
                        case INSTRUCTION_ZEXT:        instr_name = "zext";   break;
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

                        char *to_type = ir_type_str(functions[f].basicblocks[b].instructions[i]->result_type);
                        val_str = ir_value_str(((ir_instr_cast_t*) functions[f].basicblocks[b].instructions[i])->value);
                        fprintf(file, "    0x%08X %s %s to %s\n", (int) i, instr_name, val_str, to_type);
                        free(to_type);
                        free(val_str);
                    }
                    break;
                case INSTRUCTION_ISZERO: case INSTRUCTION_ISNTZERO: {
                        char *instr_name = functions[f].basicblocks[b].instructions[i]->id == INSTRUCTION_ISZERO ? "isz" : "inz";
                        val_str = ir_value_str(((ir_instr_cast_t*) functions[f].basicblocks[b].instructions[i])->value);
                        fprintf(file, "    0x%08X %s %s\n", (int) i, instr_name, val_str);
                        free(val_str);
                    }
                    break;
                case INSTRUCTION_AND:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "and");
                    break;
                case INSTRUCTION_OR:
                    ir_dump_math_instruction(file, (ir_instr_math_t*) functions[f].basicblocks[b].instructions[i], i, "or");
                    break;
                case INSTRUCTION_SIZEOF: {
                        char *typename = ir_type_str(((ir_instr_sizeof_t*) functions[f].basicblocks[b].instructions[i])->type);
                        fprintf(file, "    0x%08X sizeof %s\n", (int) i, typename);
                        free(typename);
                    }
                    break;
                case INSTRUCTION_OFFSETOF: {
                        char *typename = ir_type_str(((ir_instr_offsetof_t*) functions[f].basicblocks[b].instructions[i])->type);
                        fprintf(file, "    0x%08X offsetof %s\n", (int) i, typename);
                        free(typename);
                    }
                    break;
                case INSTRUCTION_VARZEROINIT:
                    fprintf(file, "    0x%08X varzi 0x%08X\n", (int) i, (int) ((ir_instr_varptr_t*) functions[f].basicblocks[b].instructions[i])->index);
                    break;
                case INSTRUCTION_BIT_COMPLEMENT:
                    val_str = ir_value_str(((ir_instr_load_t*) functions[f].basicblocks[b].instructions[i])->value);
                    fprintf(file, "    0x%08X compl %s\n", (int) i, val_str);
                    free(val_str);
                    break;
                case INSTRUCTION_NEGATE:
                    val_str = ir_value_str(((ir_instr_load_t*) functions[f].basicblocks[b].instructions[i])->value);
                    fprintf(file, "    0x%08X neg %s\n", (int) i, val_str);
                    free(val_str);
                    break;
                case INSTRUCTION_FNEGATE:
                    val_str = ir_value_str(((ir_instr_load_t*) functions[f].basicblocks[b].instructions[i])->value);
                    fprintf(file, "    0x%08X fneg %s\n", (int) i, val_str);
                    free(val_str);
                    break;
                case INSTRUCTION_PHI2: {
                        ir_instr_phi2_t *phi2 = (ir_instr_phi2_t*) functions[f].basicblocks[b].instructions[i];
                        char *a = ir_value_str(phi2->a);
                        char *b = ir_value_str(phi2->b);
                        fprintf(file, "    0x%08X phi2 |%d| -> %s, |%d| -> %s\n", (int) i, (int) phi2->block_id_a, a, (int) phi2->block_id_b, b);
                        free(a);
                        free(b);
                    }
                    break;
                case INSTRUCTION_SWITCH: {
                        ir_instr_switch_t *switch_instr = (ir_instr_switch_t*) functions[f].basicblocks[b].instructions[i];
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
                        ir_type_t *alloc_result_type = ((ir_instr_alloc_t*) functions[f].basicblocks[b].instructions[i])->result_type;
                        if(alloc_result_type->kind != TYPE_KIND_POINTER){
                            fprintf(file, "    0x%08X alloc (malformed)\n", (int) i);
                        } else {
                            char *typename = ir_type_str(alloc_result_type->extra);
                            fprintf(file, "    0x%08X alloc %s\n", (int) i, typename);
                            free(typename);
                        }
                    }
                    break;
                case INSTRUCTION_STACK_SAVE:
                    fprintf(file, "    0x%08X ssave\n", (int) i);
                    break;
                case INSTRUCTION_STACK_RESTORE:
                    val_str = ir_value_str(((ir_instr_stack_restore_t*) functions[f].basicblocks[b].instructions[i])->stack_pointer);
                    fprintf(file, "    0x%08X srestore %s\n", (int) i, val_str);
                    free(val_str);
                    break;
                default:
                    printf("Unknown instruction id 0x%08X when dumping ir module\n", (int) functions[f].basicblocks[b].instructions[i]->id);
                    fprintf(file, "    0x%08X <unknown instruction>\n", (int) i);
                }
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

void ir_dump_call_instruction(FILE *file, ir_instr_call_t *instruction, int i, const char *real_name){
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
    fprintf(file, "    0x%08X call %s \"%s\" (%s) %s\n", i, implementation_buffer, real_name, call_args, call_result_type);
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

void ir_module_init(ir_module_t *ir_module, length_t funcs_capacity, length_t globals_length){
    ir_pool_init(&ir_module->pool);

    ir_module->funcs = malloc(sizeof(ir_func_t) * funcs_capacity);
    ir_module->funcs_length = 0;
    ir_module->funcs_capacity = funcs_capacity;
    ir_module->func_mappings = NULL;
    ir_module->func_mappings_length = 0;
    ir_module->func_mappings_capacity = 0;
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
    ir_type_var_stack_init(&ir_module->type_var_stack);
    ir_gen_sf_cache_init(&ir_module->sf_cache);

    // Initialize common data
    ir_module->common.ir_usize = NULL;
    ir_module->common.ir_usize_ptr = NULL;
    ir_module->common.ir_bool = NULL;
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
    ir_type_var_stack_free(&ir_module->type_var_stack);
    ir_gen_sf_cache_free(&ir_module->sf_cache);
}

void ir_module_free_funcs(ir_func_t *funcs, length_t funcs_length){
    for(length_t f = 0; f != funcs_length; f++){
        for(length_t b = 0; b != funcs[f].basicblocks_length; b++){
            free(funcs[f].basicblocks[b].instructions);
        }
        free(funcs[f].basicblocks);
        free(funcs[f].argument_types);

        if(funcs[f].scope != NULL){
            bridge_scope_free(funcs[f].scope);
            free(funcs[f].scope);
        }
    }
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

unsigned long long ir_value_uniqueness_value(ir_value_t *value){
    if(value->value_type != VALUE_TYPE_LITERAL){
        printf("INTERNAL ERROR: ir_value_uniqueness_value received a value that isn't a constant literal\n");
        return 0xFFFFFFFF;
    }

    switch(value->type->kind){
    case TYPE_KIND_BOOLEAN: return *((bool*) value->extra);
    case TYPE_KIND_S8:      return *((char*) value->extra);
    case TYPE_KIND_U8:      return *((unsigned char*) value->extra);
    case TYPE_KIND_S16:     return *((int*) value->extra);
    case TYPE_KIND_U16:     return *((unsigned int*) value->extra);
    case TYPE_KIND_S32:     return *((long*) value->extra);
    case TYPE_KIND_U32:     return *((unsigned long*) value->extra);
    case TYPE_KIND_S64:     return *((long*) value->extra);
    case TYPE_KIND_U64:     return *((unsigned long*) value->extra);
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
