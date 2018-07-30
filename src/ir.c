
#include "ir.h"
#include "color.h"

char* ir_type_str(ir_type_t *type){
    // NOTE: Returns allocated string of that type
    // NOTE: This function is recusive

    #define RET_CLONE_STR_MACRO(d, s) { \
        memory = malloc(s); memcpy(memory, d, s); return memory; \
    }

    char *memory;
    char *chained;
    length_t chained_length;

    switch(type->kind){
    case TYPE_KIND_NONE:
        RET_CLONE_STR_MACRO("__nul_type_kind", 16);
    case TYPE_KIND_POINTER:
        chained = ir_type_str((ir_type_t*) type->extra);
        chained_length = strlen(chained);
        memory = malloc(chained_length + 2);
        memcpy(memory, "*", 1);
        memcpy(&memory[1], chained, chained_length + 1);
        free(chained);
        return memory;
    case TYPE_KIND_S8:      RET_CLONE_STR_MACRO("s8", 3);
    case TYPE_KIND_U8:      RET_CLONE_STR_MACRO("u8", 3);
    case TYPE_KIND_S16:     RET_CLONE_STR_MACRO("s16", 4);
    case TYPE_KIND_U16:     RET_CLONE_STR_MACRO("u16", 4);
    case TYPE_KIND_S32:     RET_CLONE_STR_MACRO("s32", 4);
    case TYPE_KIND_U32:     RET_CLONE_STR_MACRO("u32", 4);
    case TYPE_KIND_S64:     RET_CLONE_STR_MACRO("s64", 4);
    case TYPE_KIND_U64:     RET_CLONE_STR_MACRO("u64", 4);
    case TYPE_KIND_HALF:    RET_CLONE_STR_MACRO("h", 2);
    case TYPE_KIND_FLOAT:   RET_CLONE_STR_MACRO("f", 2);
    case TYPE_KIND_DOUBLE:  RET_CLONE_STR_MACRO("d", 2);
    case TYPE_KIND_BOOLEAN: RET_CLONE_STR_MACRO("bool", 5);
    case TYPE_KIND_VOID:    RET_CLONE_STR_MACRO("void", 5);
    case TYPE_KIND_FUNCPTR: RET_CLONE_STR_MACRO("__funcptr_type_kind", 20);
    case TYPE_KIND_FIXED_ARRAY: {
        ir_type_extra_fixed_array_t *fixed = (ir_type_extra_fixed_array_t*) type->extra;
        chained = ir_type_str(fixed->subtype);
        memory = malloc(strlen(chained) + 24);
        sprintf(memory, "[%d] %s", (int) fixed->length, chained);
        free(chained);
        return memory;
    }
    default: RET_CLONE_STR_MACRO("__unk_type_kind", 16);
    }

    #undef RET_CLONE_STR_MACRO
}

char* ir_value_str(ir_value_t *value){
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
            sprintf(value_str, "%s %s", typename, *((double*) value->extra) ? "true" : "false");
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
                    if(string_data[s] > 0x1F || string_data[s] == '\\') special_characters++;
                }

                value_str = malloc(string_data_length + special_characters + 3);
                value_str[0] = '\'';
                value_str[string_data_length + 2] = '\'';
                value_str[string_data_length + 3] = '\0';

                for(length_t c = 0; c != string_data_length; c++){
                    if(string_data[c] > 0x1F || string_data[c] == '\\') value_str[put_index++] = string_data[c];
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
    case VALUE_TYPE_NULLPTR:
        value_str = malloc(5);
        memcpy(value_str, "null", 5);
        free(typename); // Typename isn't used, because it will always be 'ptr'
        return value_str;
    default:
        printf("INTERNAL ERROR: Unexpected value type of value in ir_value_str function\n");
        return NULL;
    }

    return NULL; // Should never get here
}

bool ir_type_map_find(ir_type_map_t *type_map, char *name, ir_type_t **type_ptr){
    // Does a binary search on the type map to find the requested type by name
    // NOTE: Returns whether or not an error occured (false == success)

    ir_type_mapping_t *mappings = type_map->mappings;
    length_t mappings_length = type_map->mappings_length;
    int first = 0, middle, last = mappings_length - 1, comparison;

    while(first <= last){
        middle = (first + last) / 2;
        comparison = strcmp(mappings[middle].name, name);

        if(comparison == 0){
            *type_ptr = &mappings[middle].type;
            return false;
        }
        else if(comparison > 0) last = middle - 1;
        else first = middle + 1;
    }

    return true; // Error occured
}

bool ir_var_map_find(ir_var_map_t *var_map, char *name, length_t *out_index){
    // NOTE: Returns whether or not an error occured (false == success)

    ir_var_mapping_t *mappings = var_map->mappings;
    length_t mappings_length = var_map->mappings_length;
    length_t name_size = strlen(name) + 1;

    for(length_t i = 0; i != mappings_length; i++){
        if(memcmp(mappings[i].name, name, name_size) == 0){
            *out_index = i;
            return false; // No error
        }
    }

    return true; // Error occured
}

bool ir_types_identical(ir_type_t *a, ir_type_t *b){
    // NOTE: Returns true if the two types are identical
    // NOTE: The two types must match element to element to be considered identical
    // [pointer] [pointer] [u8]    [pointer] [pointer] [s32]  -> false
    // [pointer] [u8]              [pointer] [u8]             -> true

    if(a->kind != b->kind) return false;

    switch(a->kind){
    case TYPE_KIND_POINTER:
        return ir_types_identical((ir_type_t*) a->extra, (ir_type_t*) b->extra);
    case TYPE_KIND_UNION:
        if(((ir_type_extra_composite_t*) a->extra)->subtypes_length != ((ir_type_extra_composite_t*) b->extra)->subtypes_length) return false;
        for(length_t i = 0; i != ((ir_type_extra_composite_t*) a->extra)->subtypes_length; i++){
            if(!ir_types_identical(((ir_type_extra_composite_t*) a->extra)->subtypes[i], ((ir_type_extra_composite_t*) b->extra)->subtypes[i])) return false;
        }
        return true;
    case TYPE_KIND_STRUCTURE:
        if(((ir_type_extra_composite_t*) a->extra)->subtypes_length != ((ir_type_extra_composite_t*) b->extra)->subtypes_length) return false;
        for(length_t i = 0; i != ((ir_type_extra_composite_t*) a->extra)->subtypes_length; i++){
            if(!ir_types_identical(((ir_type_extra_composite_t*) a->extra)->subtypes[i], ((ir_type_extra_composite_t*) b->extra)->subtypes[i])) return false;
        }
        return true;
    }

    return true;
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

    for(length_t f = 0; f != functions_length; f++){
        // Print prototype
        char *ret_type_str = ir_type_str(functions[f].return_type);
        fprintf(file, "fn %s -> %s\n", functions[f].name, ret_type_str);

        // Count the total number of instructions
        length_t total_instructions = 0;
        for(length_t b = 0; b != functions[f].basicblocks_length; b++) total_instructions += functions[f].basicblocks[b].instructions_length;

        // Print functions statistics
        if(!(functions[f].traits & IR_FUNC_FOREIGN))
            fprintf(file, "    {%d BBs, %d INSTRs, %d VARs}0\n", (int) functions[f].basicblocks_length, (int) total_instructions, (int) functions[f].var_map.mappings_length);

        for(length_t v = 0; v != functions[f].var_map.mappings_length; v++){
            char *type_str = ir_type_str(functions[f].var_map.mappings[v].var.type);
            fprintf(file, "    [0x%08X %s]\n", (int) v, type_str);
            free(type_str);
        }

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
                case INSTRUCTION_CALL:
                    ir_dump_call_instruction(file, (ir_instr_call_t*) functions[f].basicblocks[b].instructions[i], i);
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
                    if(((ir_instr_func_address_t*) functions[f].basicblocks[b].instructions[i])->name == NULL)
                        fprintf(file, "    0x%08X funcaddr 0x%X\n", (unsigned int) i, (int) ((ir_instr_func_address_t*) functions[f].basicblocks[b].instructions[i])->func_id);
                    else
                        fprintf(file, "    0x%08X funcaddr %s\n", (unsigned int) i, ((ir_instr_func_address_t*) functions[f].basicblocks[b].instructions[i])->name);
                    break;
                case INSTRUCTION_BITCAST: case INSTRUCTION_ZEXT:
                case INSTRUCTION_FEXT: case INSTRUCTION_TRUNC:
                case INSTRUCTION_FTRUNC: case INSTRUCTION_INTTOPTR:
                case INSTRUCTION_PTRTOINT: case INSTRUCTION_FPTOUI:
                case INSTRUCTION_FPTOSI: case INSTRUCTION_UITOFP:
                case INSTRUCTION_SITOFP: case INSTRUCTION_REINTERPRET:
                case INSTRUCTION_ISNTZERO: {
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
                        case INSTRUCTION_ISNTZERO:    instr_name = "inz";    break;
                        case INSTRUCTION_REINTERPRET: instr_name = "reinterp";  break;
                        }

                        char *to_type = ir_type_str(functions[f].basicblocks[b].instructions[i]->result_type);
                        val_str = ir_value_str(((ir_instr_cast_t*) functions[f].basicblocks[b].instructions[i])->value);
                        fprintf(file, "    0x%08X %s %s to %s\n", (int) i, instr_name, val_str, to_type);
                        free(to_type);
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
                default:
                    printf("Unknown instruction id 0x%08X when dumping ir module\n", (int) functions[f].basicblocks[b].instructions[i]->id);
                    fprintf(file, "    0x%08X <unknown instruction>\n", (int) i);
                }
            }
        }

        free(ret_type_str);
    }
}

void ir_dump_math_instruction(FILE *file, ir_instr_math_t *instruction, int i, const char *instruction_name){
    char *val_str_1 = ir_value_str(instruction->a);
    char *val_str_2 = ir_value_str(instruction->b);
    fprintf(file, "    0x%08X %s %s, %s\n", i, instruction_name, val_str_1, val_str_2);
    free(val_str_1);
    free(val_str_2);
}

void ir_dump_call_instruction(FILE *file, ir_instr_call_t *instruction, int i){
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

    fprintf(file, "    0x%08X call adept_%X(%s) %s\n", i, (int) instruction->func_id, call_args, call_result_type);
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

void ir_module_free(ir_module_t *ir_module){
    ir_module_free_funcs(ir_module->funcs, ir_module->funcs_length);
    free(ir_module->funcs);
    free(ir_module->func_mappings);
    free(ir_module->methods);
    free(ir_module->type_map.mappings);
    free(ir_module->globals);
    ir_pool_free(&ir_module->pool);
}

void ir_module_free_funcs(ir_func_t *funcs, length_t funcs_length){
    for(length_t f = 0; f != funcs_length; f++){
        for(length_t b = 0; b != funcs[f].basicblocks_length; b++){
            free(funcs[f].basicblocks[b].instructions);
        }
        free(funcs[f].basicblocks);
        free(funcs[f].argument_types);
        free(funcs[f].var_map.mappings);
    }
}

void ir_pool_init(ir_pool_t *pool){
    pool->fragments = malloc(sizeof(ir_pool_fragment_t) * 4);
    pool->fragments_length = 1;
    pool->fragments_capacity = 4;
    pool->fragments[0].memory = malloc(512);
    pool->fragments[0].used = 0;
    pool->fragments[0].capacity = 512;
}

void* ir_pool_alloc(ir_pool_t *pool, length_t bytes){
    // NOTE: Allocates memory in an ir_pool_t
    ir_pool_fragment_t *recent_fragment = &pool->fragments[pool->fragments_length - 1];

    while(recent_fragment->used + bytes >= recent_fragment->capacity){
        if(pool->fragments_length == pool->fragments_capacity){
            pool->fragments_capacity *= 2;
            ir_pool_fragment_t *new_fragments = malloc(sizeof(ir_pool_fragment_t) * pool->fragments_capacity);
            memcpy(new_fragments, pool->fragments, sizeof(ir_pool_fragment_t) * pool->fragments_length);
            free(pool->fragments);
            pool->fragments = new_fragments;

            // Update recent_fragment pointer to point into new array
            recent_fragment = &pool->fragments[pool->fragments_length - 1];
        }

        ir_pool_fragment_t *new_fragment = &pool->fragments[pool->fragments_length++];
        new_fragment->capacity = recent_fragment->capacity * 2;
        new_fragment->memory = malloc(new_fragment->capacity);
        new_fragment->used = 0;
        recent_fragment = new_fragment;
    }

    void *memory = (void*) &((char*) recent_fragment->memory)[recent_fragment->used];
    recent_fragment->used += bytes;
    return memory;
}

void ir_pool_free(ir_pool_t *pool){
    for(length_t f = 0; f != pool->fragments_length; f++){
        free(pool->fragments[f].memory);
    }
    free(pool->fragments);
}

void ir_pool_snapshot_capture(ir_pool_t *pool, ir_pool_snapshot_t *snapshot){
    snapshot->used = pool->fragments[pool->fragments_length - 1].used;
    snapshot->fragments_length = pool->fragments_length;
}

void ir_pool_snapshot_restore(ir_pool_t *pool, ir_pool_snapshot_t *snapshot){
    for(length_t f = pool->fragments_length; f != snapshot->fragments_length; f--){
        free(pool->fragments[f - 1].memory);
    }

    pool->fragments_length = snapshot->fragments_length;
    pool->fragments[snapshot->fragments_length - 1].used = snapshot->used;
}

// (For 64 bit systems)
unsigned int global_type_kind_sizes_64[] = {
     0, // TYPE_KIND_NONE
    64, // TYPE_KIND_POINTER
     8, // TYPE_KIND_S8
    16, // TYPE_KIND_S16
    32, // TYPE_KIND_S32
    64, // TYPE_KIND_S64
     8, // TYPE_KIND_U8
    16, // TYPE_KIND_U16
    32, // TYPE_KIND_U32
    64, // TYPE_KIND_U64
    16, // TYPE_KIND_HALF
    32, // TYPE_KIND_FLOAT
    64, // TYPE_KIND_DOUBLE
     1, // TYPE_KIND_BOOLEAN
     0, // TYPE_KIND_UNION
     0, // TYPE_KIND_STRUCTURE
     0, // TYPE_KIND_VOID
    64, // TYPE_KIND_FUNCPTR
     0, // TYPE_KIND_FIXED_ARRAY
};

bool global_type_kind_signs[] = { // (0 == unsigned, 1 == signed)
    0, // TYPE_KIND_NONE
    0, // TYPE_KIND_POINTER
    1, // TYPE_KIND_S8
    1, // TYPE_KIND_S16
    1, // TYPE_KIND_S32
    1, // TYPE_KIND_S64
    0, // TYPE_KIND_U8
    0, // TYPE_KIND_U16
    0, // TYPE_KIND_U32
    0, // TYPE_KIND_U64
    1, // TYPE_KIND_HALF
    1, // TYPE_KIND_FLOAT
    1, // TYPE_KIND_DOUBLE
    0, // TYPE_KIND_BOOLEAN
    0, // TYPE_KIND_UNION
    0, // TYPE_KIND_STRUCTURE
    0, // TYPE_KIND_VOID
    0, // TYPE_KIND_FUNCPTR
    0, // TYPE_KIND_FIXED_ARRAY
};
