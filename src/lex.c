
#include "lex.h"
#include "pkg.h"
#include "util.h"
#include "search.h"
#include "filename.h"

int lex(compiler_t *compiler, object_t *object){
    FILE *file = fopen(object->filename, "r");

    token_t *t;
    char *buffer;
    int line, column;
    length_t buffer_size;
    lex_state_t lex_state;
    tokenlist_t *tokenlist = &object->tokenlist;

    if(file == NULL){
        redprintf("The file '%s' doesn't exist or can't be accessed\n", object->filename);
        return 1;
    }

    fseek(file, 0, SEEK_END);
    buffer_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    tokenlist->tokens = malloc(sizeof(token_t) * 1024);
    tokenlist->length = 0;
    tokenlist->capacity = 1024;
    tokenlist->sources = malloc(sizeof(source_t) * 1024);

    buffer = malloc(buffer_size + 2);
    buffer_size = fread(buffer, 1, buffer_size, file);
    fclose(file);

    buffer[buffer_size] = '\n';   // Append newline to flush everything
    buffer[++buffer_size] = '\0'; // Terminate the string for debug purposes
    object->buffer = buffer;      // Pass ownership to object instance

    // By this point we have a buffer and a tokenlist
    object->compilation_stage = COMPILATION_STAGE_TOKENLIST;

    lex_state_init(&lex_state); // NOTE: lex_state_free() must be called before exiting this function

    lex_state.buildup = malloc(256);
    lex_state.buildup_capacity = 256;

    token_t *tokens = tokenlist->tokens;
    source_t *sources = tokenlist->sources;
    length_t object_index = object->index;
    char tmp;

    for(length_t i = 0; i != buffer_size; i++){
        if(tokenlist->length == tokenlist->capacity){
            token_t* new_tokens = malloc(sizeof(token_t) * tokenlist->capacity * 2);
            source_t *new_sources = malloc(sizeof(source_t) * tokenlist->capacity * 2);
            memcpy(new_tokens, tokenlist->tokens, sizeof(token_t) * tokenlist->capacity);
            memcpy(new_sources, tokenlist->sources, sizeof(source_t) * tokenlist->capacity);
            free(tokenlist->tokens);
            free(tokenlist->sources);

            tokenlist->tokens = new_tokens;
            tokenlist->sources = new_sources;
            tokenlist->capacity *= 2;
            tokens = tokenlist->tokens;
            sources = tokenlist->sources;
        }

        // Macro to map a character to a single token
        #define LEX_SINGLE_TOKEN_MAPPING_MACRO(token_id_mapping) { \
            sources[tokenlist->length].index = i; \
            sources[tokenlist->length].object_index = object_index; \
            t = &(tokens[tokenlist->length++]); \
            t->id = token_id_mapping; \
            t->data = NULL; \
        }

        // Macro to map a character to a single lex state
        #define LEX_SINGLE_STATE_MAPPING_MACRO(state_mapping) { \
            sources[tokenlist->length].index = i; \
            sources[tokenlist->length].object_index = object_index; \
            lex_state.state = state_mapping; \
        }

        // Macro to add a token depending on whether an optional character is present
        // (can be used in non LEX_STATE_IDLE states)
        #define LEX_OPTIONAL_MOD_TOKEN_MAPPING(optional_character, if_mod_present, if_mod_absent) { \
            t = &(tokens[tokenlist->length++]); \
            lex_state.state = LEX_STATE_IDLE; \
            t->data = NULL; \
            if(buffer[i] == optional_character) t->id = if_mod_present; \
            else { t->id = if_mod_absent; i--; } \
        }

        switch(lex_state.state){
        case LEX_STATE_IDLE:
            switch(buffer[i]){
            case ' ': case '\t': break;
            case '+': LEX_SINGLE_STATE_MAPPING_MACRO(LEX_STATE_ADD);        break;
            case '-': LEX_SINGLE_STATE_MAPPING_MACRO(LEX_STATE_SUBTRACT);   break;
            case '*': LEX_SINGLE_STATE_MAPPING_MACRO(LEX_STATE_MULTIPLY);   break;
            case '/': LEX_SINGLE_STATE_MAPPING_MACRO(LEX_STATE_DIVIDE);     break;
            case '%': LEX_SINGLE_STATE_MAPPING_MACRO(LEX_STATE_MODULUS);    break;
            case '&': LEX_SINGLE_STATE_MAPPING_MACRO(LEX_STATE_UBERAND);    break;
            case '|': LEX_SINGLE_STATE_MAPPING_MACRO(LEX_STATE_UBEROR);     break;
            case '=': LEX_SINGLE_STATE_MAPPING_MACRO(LEX_STATE_EQUALS);     break;
            case '<': LEX_SINGLE_STATE_MAPPING_MACRO(LEX_STATE_LESS);       break;
            case '>': LEX_SINGLE_STATE_MAPPING_MACRO(LEX_STATE_GREATER);    break;
            case '!': LEX_SINGLE_STATE_MAPPING_MACRO(LEX_STATE_NOT);        break;
            //--------------------------------------------------------------------
            case '(': LEX_SINGLE_TOKEN_MAPPING_MACRO(TOKEN_OPEN);           break;
            case ')': LEX_SINGLE_TOKEN_MAPPING_MACRO(TOKEN_CLOSE);          break;
            case '{': LEX_SINGLE_TOKEN_MAPPING_MACRO(TOKEN_BEGIN);          break;
            case '}': LEX_SINGLE_TOKEN_MAPPING_MACRO(TOKEN_END);            break;
            case ',': LEX_SINGLE_TOKEN_MAPPING_MACRO(TOKEN_NEXT);           break;
            case '[': LEX_SINGLE_TOKEN_MAPPING_MACRO(TOKEN_BRACKET_OPEN);   break;
            case ']': LEX_SINGLE_TOKEN_MAPPING_MACRO(TOKEN_BRACKET_CLOSE);  break;
            case ';': LEX_SINGLE_TOKEN_MAPPING_MACRO(TOKEN_TERMINATE_JOIN); break;
            case ':': LEX_SINGLE_TOKEN_MAPPING_MACRO(TOKEN_COLON);          break;
            case '\n': LEX_SINGLE_TOKEN_MAPPING_MACRO(TOKEN_NEWLINE);       break;
            case '.':
                sources[tokenlist->length].index = i;
                sources[tokenlist->length].object_index = object_index;
                t = &(tokens[tokenlist->length++]);
                t->data = NULL;
                if(buffer[i + 1] == '.' && buffer[i + 2] == '.'){
                    t->id = TOKEN_ELLIPSIS; i += 2;
                } else {
                    t->id = TOKEN_MEMBER;
                }
                break;
            case '"':
                LEX_SINGLE_STATE_MAPPING_MACRO(LEX_STATE_STRING);
                lex_state.buildup_length = 0; break;
            case '\'':
                LEX_SINGLE_STATE_MAPPING_MACRO(LEX_STATE_CSTRING);
                lex_state.buildup_length = 0; break;
            default:
                // Test for word
                if(buffer[i] == '_' || (buffer[i] >= 65 && buffer[i] <= 90) || (buffer[i] >= 97 && buffer[i] <= 122)){
                    sources[tokenlist->length].index = i;
                    sources[tokenlist->length].object_index = object_index;
                    lex_state.buildup_length = 1;
                    lex_state.buildup[0] = buffer[i];
                    lex_state.state = LEX_STATE_WORD;
                    break;
                }

                // Test for number
                if(buffer[i] >= '0' && buffer[i] <= '9'){
                    if(lex_state.buildup == NULL){
                        lex_state.buildup = malloc(256);
                        lex_state.buildup_capacity = 256;
                    }
                    sources[tokenlist->length].index = i;
                    sources[tokenlist->length].object_index = object_index;
                    lex_state.buildup_length = 1;
                    lex_state.buildup[0] = buffer[i];
                    lex_state.state = LEX_STATE_NUMBER;
                    lex_state.is_hexadecimal = false;
                    break;
                }

                lex_get_location(buffer, i, &line, &column);
                redprintf("%s:%d:%d: Unrecognized symbol '%c' (0x%02X)\n", filename_name_const(object->filename), line, column, buffer[i], (int) buffer[i]);

                source_t here; here.index = i;
                here.object_index = object->index;
                compiler_print_source(compiler, line, column, here);
                lex_state_free(&lex_state);
                return 1;
            }
            break;
        case LEX_STATE_WORD:
            tmp = buffer[i];
            if(tmp == '_' || (tmp >= 65 && tmp <= 90) || (tmp >= 97 && tmp <= 122) || (tmp >= '0' && tmp <= '9')){
                expand((void**) &lex_state.buildup, sizeof(char), lex_state.buildup_length, &lex_state.buildup_capacity, 1, 256);
                lex_state.buildup[lex_state.buildup_length++] = buffer[i];
            } else {
                // NOTE: MUST be pre sorted alphabetically (used for string_search)
                //         Make sure to update values inside token.h and token.c after modifying this list
                const length_t keywords_length = 45;
                const char * const keywords[] = {
                    "alias", "and", "break", "case", "cast", "continue", "dangerous", "def", "default", "defer",
                    "delete", "dynamic", "else", "enum", "external", "false", "for", "foreign", "func", "funcptr",
                    "global", "if", "import", "in", "inout", "link", "new", "null", "or", "out",
                    "packed", "pragma", "private", "public", "return", "sizeof", "static", "stdcall", "struct", "switch",
                    "true", "undef", "unless", "until", "while"
                };

                // Terminate string buildup buffer
                expand((void**) &lex_state.buildup, sizeof(char), lex_state.buildup_length, &lex_state.buildup_capacity, 1, 256);
                lex_state.buildup[lex_state.buildup_length] = '\0';

                // Search for string inside keyword list
                int array_index = binary_string_search(keywords, keywords_length, lex_state.buildup);

                if(array_index == -1){
                    // Isn't a keyword, just an identifier
                    t = &(tokens[tokenlist->length++]);
                    t->id = TOKEN_WORD;
                    t->data = malloc(lex_state.buildup_length + 1);
                    memcpy(t->data, lex_state.buildup, lex_state.buildup_length);
                    ((char*) t->data)[lex_state.buildup_length] = '\0';
                } else {
                    // Is a keyword, figure out token index from array index
                    t = &(tokens[tokenlist->length++]);
                    t->id = 0x00000040 + (unsigned int) array_index; // Values 0x00000040..0x0000005F are reserved for keywords
                    t->data = NULL;
                }

                i--; lex_state.state = LEX_STATE_IDLE;
            }
            break;
        case LEX_STATE_STRING:
            if(buffer[i] == '\"' && !(lex_state.buildup_length != 0 && lex_state.buildup[lex_state.buildup_length-1] == '\\')){
                // End of string literal
                t = &(tokens[tokenlist->length++]);
                t->id = TOKEN_STRING;
                t->data = malloc(lex_state.buildup_length + 1);
                memcpy(t->data, lex_state.buildup, lex_state.buildup_length);
                ((char*) t->data)[lex_state.buildup_length] = '\0';
                lex_state.state = LEX_STATE_IDLE;
                break;
            }

            // Check for cheeky null character
            if(buffer[i] == '\0'){
                lex_get_location(buffer, i, &line, &column);
                redprintf("%s:%d:%d: Raw null character found in string\n", filename_name_const(object->filename), line, column);
                lex_state_free(&lex_state);
                return 1;
            }

            // Add the character to the string
            lex_state.buildup[lex_state.buildup_length++] = buffer[i];
            break;
        case LEX_STATE_CSTRING:
            if(buffer[i] == '\''){
                // End of string literal
                t = &(tokens[tokenlist->length++]);

                // Character literals
                if(lex_state.buildup_length == 1){
                    if(buffer[i + 1] == 'u' && buffer[i + 2] == 'b'){
                        t->id = TOKEN_UBYTE;
                        t->data = malloc(sizeof(unsigned char));
                        *((unsigned char*) t->data) = lex_state.buildup[0];
                        lex_state.state = LEX_STATE_IDLE;
                        i += 2; break;
                    } else if(buffer[i + 1] == 's' && buffer[i + 2] == 'b'){
                        t->id = TOKEN_BYTE;
                        t->data = malloc(sizeof(char));
                        *((char*) t->data) = lex_state.buildup[0];
                        lex_state.state = LEX_STATE_IDLE;
                        i += 2; break;
                    }
                }

                // Regular cstring
                t->id = TOKEN_CSTRING;
                t->data = malloc(lex_state.buildup_length + 1);
                memcpy(t->data, lex_state.buildup, lex_state.buildup_length);
                ((char*) t->data)[lex_state.buildup_length] = '\0';
                lex_state.state = LEX_STATE_IDLE;
                break;
            }

            expand((void**) &lex_state.buildup, sizeof(char), lex_state.buildup_length, &lex_state.buildup_capacity, 1, 256);

            if(buffer[i] == '\\'){
                switch(buffer[++i]){
                case 'n': lex_state.buildup[lex_state.buildup_length++] = '\n'; break;
                case 'r': lex_state.buildup[lex_state.buildup_length++] = '\r'; break;
                case 't': lex_state.buildup[lex_state.buildup_length++] = '\t'; break;
                case 'b': lex_state.buildup[lex_state.buildup_length++] = '\b'; break;
                case '\'': lex_state.buildup[lex_state.buildup_length++] = '\''; break;
                case '\\': lex_state.buildup[lex_state.buildup_length++] = '\\'; break;
                default:
                    lex_get_location(buffer, i, &line, &column);
                    redprintf("%s:%d:%d: Unknown string escape sequence '\\%c'\n", filename_name_const(object->filename), line, column, buffer[i]);
                    lex_state_free(&lex_state);
                    return 1;
                }
            } else {
                // Check for cheeky null character
                if(buffer[i] == '\0'){
                    lex_get_location(buffer, i, &line, &column);
                    redprintf("%s:%d:%d: Raw null character found in string\n", filename_name_const(object->filename), line, column);
                    lex_state_free(&lex_state);
                    return 1;
                }

                lex_state.buildup[lex_state.buildup_length++] = buffer[i];
            }
            break;
        case LEX_STATE_EQUALS:   LEX_OPTIONAL_MOD_TOKEN_MAPPING('=', TOKEN_EQUALS, TOKEN_ASSIGN);             break;
        case LEX_STATE_NOT:      LEX_OPTIONAL_MOD_TOKEN_MAPPING('=', TOKEN_NOTEQUALS, TOKEN_NOT);             break;
        case LEX_STATE_ADD:      LEX_OPTIONAL_MOD_TOKEN_MAPPING('=', TOKEN_ADDASSIGN, TOKEN_ADD);             break;
        case LEX_STATE_MULTIPLY: LEX_OPTIONAL_MOD_TOKEN_MAPPING('=', TOKEN_MULTIPLYASSIGN, TOKEN_MULTIPLY);   break;
        case LEX_STATE_MODULUS:  LEX_OPTIONAL_MOD_TOKEN_MAPPING('=', TOKEN_MODULUSASSIGN, TOKEN_MODULUS);     break;
        case LEX_STATE_LESS:     LEX_OPTIONAL_MOD_TOKEN_MAPPING('=', TOKEN_LESSTHANEQ, TOKEN_LESSTHAN);       break;
        case LEX_STATE_GREATER:  LEX_OPTIONAL_MOD_TOKEN_MAPPING('=', TOKEN_GREATERTHANEQ, TOKEN_GREATERTHAN); break;
        case LEX_STATE_UBERAND:  LEX_OPTIONAL_MOD_TOKEN_MAPPING('&', TOKEN_UBERAND, TOKEN_ADDRESS);           break;
        case LEX_STATE_NUMBER:
            expand((void**) &lex_state.buildup, sizeof(char), lex_state.buildup_length, &lex_state.buildup_capacity, 1, 256);

            tmp = buffer[i];
            if(tmp == '_') break; // Ignore underscores in numbers

            if((tmp >= '0' && tmp <= '9') || tmp == '.' || (lex_state.is_hexadecimal
            &&((tmp >= 'A' && tmp <= 'F') || (tmp >= 'a' && tmp <= 'f'))) ){
                lex_state.buildup[lex_state.buildup_length++] = buffer[i];
            } else if(lex_state.buildup[0] == '0' && lex_state.buildup_length == 1 && (tmp == 'x' || tmp == 'X')){
                lex_state.buildup[lex_state.buildup_length++] = 'x';
                lex_state.is_hexadecimal = true;
            } else {
                expand((void**) &lex_state.buildup, sizeof(char), lex_state.buildup_length, &lex_state.buildup_capacity, 1, 256);
                lex_state.buildup[lex_state.buildup_length] = '\0';
                t = &(tokens[tokenlist->length++]);
                lex_state.state = LEX_STATE_IDLE;

                bool contains_dot = false;
                int base = lex_state.is_hexadecimal ? 16 : 10;

                if(contains_dot && lex_state.is_hexadecimal){
                    lex_get_location(buffer, i, &line, &column);
                    redprintf("%s:%d:%d: Hexadecimal numbers cannot contain dots\n", filename_name_const(object->filename), line, column);
                    lex_state_free(&lex_state);
                    return 1;
                }

                switch(buffer[i]){
                case 'u':
                    if(i != buffer_size){
                        switch(buffer[++i]){
                        case 'b':
                            t->id = TOKEN_UBYTE;
                            t->data = malloc(sizeof(unsigned char));
                            *((unsigned char*) t->data) = strtoul(lex_state.buildup, NULL, base);
                            i++; break;
                        case 's':
                            t->id = TOKEN_USHORT;
                            t->data = malloc(sizeof(unsigned short));
                            *((unsigned short*) t->data) = strtoul(lex_state.buildup, NULL, base);
                            i++; break;
                        case 'i':
                            t->id = TOKEN_UINT;
                            t->data = malloc(sizeof(unsigned long long));
                            *((unsigned long*) t->data) = strtoul(lex_state.buildup, NULL, base);
                            i++; break;
                        case 'l':
                            t->id = TOKEN_ULONG;
                            t->data = malloc(sizeof(unsigned long long));
                            *((unsigned long long*) t->data) = strtoull(lex_state.buildup, NULL, base);// NOTE: Potential loss of data
                            i++; break;
                        case 'z':
                            t->id = TOKEN_USIZE;
                            t->data = malloc(sizeof(unsigned long long));
                            *((unsigned long long*) t->data) = strtoull(lex_state.buildup, NULL, base);// NOTE: Potential loss of data
                            i++; break;
                        default:
                            lex_get_location(buffer, i, &line, &column);
                            redprintf("%s:%d:%d: Expected valid number suffix after 'u' base suffix\n", filename_name_const(object->filename), line, column);
                            lex_state_free(&lex_state);
                            return 1;
                        }
                    } else {
                        lex_get_location(buffer, i, &line, &column);
                        redprintf("%s:%d:%d: Expected valid number suffix after 'u' base suffix\n", filename_name_const(object->filename), line, column);
                        lex_state_free(&lex_state);
                        return 1;
                    }
                    break;
                case 's':
                    if(i != buffer_size){
                        switch(buffer[++i]){
                        case 'b':
                            t->id = TOKEN_BYTE;
                            t->data = malloc(sizeof(char));
                            *((char*) t->data) = strtol(lex_state.buildup, NULL, base);
                            i++; break;
                        case 's':
                            t->id = TOKEN_SHORT;
                            t->data = malloc(sizeof(short));
                            *((short*) t->data) = strtol(lex_state.buildup, NULL, base);
                            i++; break;
                        case 'i':
                            t->id = TOKEN_INT;
                            t->data = malloc(sizeof(long long));
                            *((long long*) t->data) = strtol(lex_state.buildup, NULL, base);
                            i++; break;
                        case 'l':
                            t->id = TOKEN_LONG;
                            t->data = malloc(sizeof(long long));
                            *((long long*) t->data) = strtol(lex_state.buildup, NULL, base); // NOTE: Potential loss of data
                            i++; break;
                        default:
                            t->id = TOKEN_SHORT;
                            t->data = malloc(sizeof(short));
                            *((short*) t->data) = strtol(lex_state.buildup, NULL, base);
                        }
                    } else {
                        t->id = TOKEN_SHORT;
                        t->data = malloc(sizeof(short));
                        *((short*) t->data) = strtol(lex_state.buildup, NULL, base);
                    }
                    break;
                case 'b':
                    t->id = TOKEN_BYTE;
                    t->data = malloc(sizeof(char));
                    *((char*) t->data) = strtol(lex_state.buildup, NULL, base);
                    i++; break;
                case 'i':
                    t->id = TOKEN_INT;
                    t->data = malloc(sizeof(long));
                    *((long*) t->data) = strtol(lex_state.buildup, NULL, base);
                    i++; break;
                case 'l':
                    t->id = TOKEN_LONG;
                    t->data = malloc(sizeof(long long));
                    *((long long*) t->data) = strtol(lex_state.buildup, NULL, base); // NOTE: Potential loss of data
                    i++; break;
                case 'f':
                    t->id = TOKEN_FLOAT;
                    t->data = malloc(sizeof(float));
                    *((float*) t->data) = atof(lex_state.buildup);
                    i++;
                    break;
                case 'd':
                    t->id = TOKEN_DOUBLE;
                    t->data = malloc(sizeof(double));
                    *((double*) t->data) = atof(lex_state.buildup);
                    i++;
                    break;
                default:
                    // Probably not ideal for checking but I can't think of a better way atm
                    // TODO: Clean up this messy code, maybe make a contains() function in util.c?
                    for(length_t d = 0; d != lex_state.buildup_length; d++){
                        if(lex_state.buildup[d] == '.'){
                            if(contains_dot){
                                lex_get_location(buffer, i, &line, &column);
                                redprintf("%s:%d:%d: Numbers cannot contain multiple dots\n", filename_name_const(object->filename), line, column);
                                lex_state_free(&lex_state);
                                return 1;
                            }
                            contains_dot = true;
                        }
                    }

                    if(contains_dot){
                        t->id = TOKEN_GENERIC_FLOAT;
                        t->data = malloc(sizeof(double));
                        *((double*) t->data) = atof(lex_state.buildup);
                    } else {
                        t->id = TOKEN_GENERIC_INT;
                        t->data = malloc(sizeof(long long));
                        *((long long*) t->data) = strtol(lex_state.buildup, NULL, base);
                    }
                    break;
                }
                i--;
            }
            break;
        case LEX_STATE_SUBTRACT:
            // Test for number
            if(buffer[i] >= '0' && buffer[i] <= '9'){
                if(lex_state.buildup == NULL){
                    lex_state.buildup = malloc(256);
                    lex_state.buildup_capacity = 256;
                }
                sources[tokenlist->length].index = i;
                sources[tokenlist->length].object_index = object_index;
                lex_state.buildup_length = 2;
                lex_state.buildup[0] = '-';
                lex_state.buildup[1] = buffer[i];
                lex_state.state = LEX_STATE_NUMBER;
                lex_state.is_hexadecimal = false;
                break;
            }

            t = &(tokens[tokenlist->length++]);
            lex_state.state = LEX_STATE_IDLE;
            t->data = NULL;
            if(buffer[i] == '=') t->id = TOKEN_SUBTRACTASSIGN;
            else { t->id = TOKEN_SUBTRACT; i--; }
            break;
        case LEX_STATE_DIVIDE:
            switch(buffer[i]){
            case '/':
                lex_state.state = LEX_STATE_LINECOMMENT; break;
            case '*':
                lex_state.state = LEX_STATE_LONGCOMMENT; break;
            case '=':
                lex_state.state = LEX_STATE_IDLE;
                t = &(tokens[tokenlist->length++]);
                t->id = TOKEN_DIVIDEASSIGN;
                t->data = NULL;
                break;
            default:
                lex_state.state = LEX_STATE_IDLE;
                t = &(tokens[tokenlist->length++]);
                t->id = TOKEN_DIVIDE;
                t->data = NULL;
                i--;
            }
            break;
        case LEX_STATE_LINECOMMENT:
            if(buffer[i] == '\n') { lex_state.state = LEX_STATE_IDLE; i--; }
            break;
        case LEX_STATE_LONGCOMMENT:
            if(buffer[i] == '*') lex_state.state = LEX_STATE_ENDCOMMENT;
            break;
        case LEX_STATE_ENDCOMMENT: // End Multiline Comment
            if(buffer[i] == '/') { lex_state.state = LEX_STATE_IDLE; }
            else lex_state.state = LEX_STATE_LONGCOMMENT;
            break;
        case LEX_STATE_UBEROR:
            t = &(tokens[tokenlist->length++]);
            lex_state.state = LEX_STATE_IDLE;
            t->data = NULL;
            if(buffer[i] == '|'){ t->id = TOKEN_UBEROR; }
            else {
                lex_get_location(buffer, --i, &line, &column);
                redprintf("%s:%d:%d: Unrecognized symbol '%c' (0x%02X)\n", filename_name_const(object->filename), line, column, buffer[i], (int) buffer[i]);

                source_t here;
                here.index = i;
                here.object_index = object->index;
                compiler_print_source(compiler, line, column, here);
                lex_state_free(&lex_state);
            }
            break;
        }

        #undef LEX_SINGLE_TOKEN_MAPPING_MACRO
        #undef LEX_SINGLE_STATE_MAPPING_MACRO
    }

    lex_state_free(&lex_state);

    if(compiler->traits & COMPILER_MAKE_PACKAGE){
        if(compiler_create_package(compiler, object) == 0){
            compiler->result_flags |= COMPILER_RESULT_SUCCESS;
        }
        return 1;
    }

    return 0;
}

void lex_state_init(lex_state_t *lex_state){
    lex_state->state = LEX_STATE_IDLE;
    lex_state->buildup = NULL;
    lex_state->buildup_length = 0;
    lex_state->buildup_capacity = 0;
}

void lex_state_free(lex_state_t *lex_state){
    free(lex_state->buildup);
}

void lex_get_location(const char *buffer, length_t i, int *line, int *column){
    // NOTE: Expects i to be pointed at the character that caused the error or is the area of interest

    if(i == 0){
        *line = 1; *column = 1; return;
    }

    length_t x, y;
    *line = 1;

    for(x = 0; x != i; x++){
        if(buffer[x] == '\n') (*line)++;
    }

    // TODO: Remove this hacky code
    if(buffer[i] == '\n' && i != 0){
        *column = 1;
        for(y = i-1; y != 0 && buffer[y] != '\n'; y--){
            if(buffer[y] == '\t'){
                (*column) += 4;
            } else {
                (*column)++;
            }
        }
    } else {
        *column = 0;
        for(y = i; y != 0 && buffer[y] != '\n'; y--){
            if(buffer[y] == '\t'){
                (*column) += 4;
            } else {
                (*column)++;
            }
        }
    }

    // NOTE: Under this is really dirty code for correcting the line & column numbers
    // NOTE: Should probably not need this if I fix up the main procedure
    // TODO: Remove this hacky code

    // Do some error correction if on line 1
    if(y == 0){
        if(buffer[0] == '\t'){
            (*column) += 4;
        } else if(buffer[0] != '\n'){
            (*column)++;
        }
    }

    if(*column == 0) *column = 1;
}
