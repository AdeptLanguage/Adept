
#include "LEX/lex.h"
#include "LEX/token.h"
#include "UTIL/color.h"
#include "UTIL/datatypes.h"

void tokenlist_print(tokenlist_t *tokenlist, const char *buffer){
    // Prints detailed information contained in tokenlist
    // NOTE: This probably should only be in like debug builds or something
    // NOTE: If buffer is NULL, line numbers and columns won't be printed

    printf("Printing %d tokens...\n", (int) tokenlist->length);

    for(length_t i = 0; i != tokenlist->length; i++){
        if(buffer != NULL){
            int line, column;
            lex_get_location(buffer, tokenlist->sources[i].index, &line, &column);
            printf("%d:%d~%d ", line, column, (int) tokenlist->sources[i].stride);
        }

        token_t *token = &tokenlist->tokens[i];

        switch(global_token_extra_format_table[token->id]){
        case TOKEN_EXTRA_DATA_FORMAT_ID_ONLY:
            printf("%s\n", global_token_name_table[token->id]);
            break;
        case TOKEN_EXTRA_DATA_FORMAT_C_STRING:
            printf("%s '%s'\n", global_token_name_table[token->id], (char*) token->data);
            break;
        case TOKEN_EXTRA_DATA_FORMAT_LEN_STRING: {
                token_string_data_t *string_data = (token_string_data_t*) token->data;
                printf("%s \"", global_token_name_table[token->id]);

                for(length_t i = 0; i != string_data->length; i++){
                    switch(string_data->array[i]){
                    case '\0': putchar('\\'); putchar('0');  break;
                    case '\t': putchar('\\'); putchar('t');  break;
                    case '\n': putchar('\\'); putchar('n');  break;
                    case '\r': putchar('\\'); putchar('r');  break;
                    case '\b': putchar('\\'); putchar('b');  break;
                    case '\e': putchar('\\'); putchar('e');  break;
                    case '\\': putchar('\\'); putchar('\\'); break;
                    case '"':  putchar('\\'); putchar('"');  break;
                    default:
                        putchar(string_data->array[i]);
                    }
                }

                printf("\"\n");
            }
            break;
        case TOKEN_EXTRA_DATA_FORMAT_MEMORY:
            printf("%s ", global_token_name_table[token->id]);
            token_print_literal_value(token);
            break;
        default:
            internalerrorprintf("tokenlist_print() got unknown extra data format\n");
        }
    }
}

void token_print_literal_value(token_t *token){
    switch(token->id){
    case TOKEN_BYTE:
        printf("%"PRId8"\n", *((adept_byte*) token->data));
        break;
    case TOKEN_UBYTE:
        printf("%"PRIu8"\n", *((adept_ubyte*) token->data));
        break;
    case TOKEN_SHORT:
        printf("%"PRId16"\n", *((adept_short*) token->data));
        break;
    case TOKEN_USHORT:
        printf("%"PRIu16"\n", *((adept_ushort*) token->data));
        break;
    case TOKEN_INT:
        printf("%"PRId32"\n", *((adept_int*) token->data));
        break;
    case TOKEN_UINT:
        printf("%"PRIu32"\n", *((adept_uint*) token->data));
        break;
    case TOKEN_LONG:
        printf("%"PRId64"\n", *((adept_long*) token->data));
        break;
    case TOKEN_ULONG:
        printf("%"PRIu64"\n", *((adept_ulong*) token->data));
        break;
    case TOKEN_USIZE:
        printf("%"PRIu64"\n", *((adept_usize*) token->data));
        break;
    case TOKEN_FLOAT:
        printf("%f\n", (double) *((adept_float*) token->data));
        break;
    case TOKEN_DOUBLE:
        printf("%f\n", *((adept_double*) token->data));
        break;
    default:
        internalerrorprintf("unknown token 0x%08X literal: this token has an unrecognized id\n", token->id);
    }
}

void tokenlist_free(tokenlist_t *tokenlist){
    for(length_t i = 0; i != tokenlist->length; i++){
        if(tokenlist->tokens[i].id == TOKEN_STRING){
            free(((token_string_data_t*) tokenlist->tokens[i].data)->array);
        }
        free(tokenlist->tokens[i].data);
    }
    free(tokenlist->tokens);
    free(tokenlist->sources);
}
