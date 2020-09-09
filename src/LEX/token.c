
#include "LEX/lex.h"
#include "LEX/token.h"
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

        switch(tokenlist->tokens[i].id){
        case TOKEN_NONE: printf("NONE: THIS TOKEN DOESN'T HAVE A VALID ID\n"); break;
        case TOKEN_WORD:
            printf("Word: \"%s\"\n", (char*) tokenlist->tokens[i].data);
            break;
        case TOKEN_STRING: {
                token_string_data_t *string_data = (token_string_data_t*) tokenlist->tokens[i].data;
                printf("String: \"");

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
        case TOKEN_CSTRING:
            printf("C String: \"%s\"\n", (char*) tokenlist->tokens[i].data);
            break;
        case TOKEN_ADD:           printf("Add\n"); break;
        case TOKEN_SUBTRACT:      printf("Subtract\n"); break;
        case TOKEN_MULTIPLY:      printf("Multiply\n"); break;
        case TOKEN_DIVIDE:        printf("Divide\n"); break;
        case TOKEN_ASSIGN:        printf("Assign\n"); break;
        case TOKEN_EQUALS:        printf("Equals\n"); break;
        case TOKEN_NOTEQUALS:     printf("Not Equals\n"); break;
        case TOKEN_LESSTHAN:      printf("Less Than\n"); break;
        case TOKEN_GREATERTHAN:   printf("Greater Than\n"); break;
        case TOKEN_LESSTHANEQ:    printf("Less Than Or Equal\n"); break;
        case TOKEN_GREATERTHANEQ: printf("Greater Than Or Equal\n"); break;
        case TOKEN_NOT:           printf("Not\n"); break;
        case TOKEN_NAMESPACE:     printf("Namespace\n"); break;
        case TOKEN_OPEN:          printf("Open\n"); break;
        case TOKEN_CLOSE:         printf("Close\n"); break;
        case TOKEN_BEGIN:         printf("Begin\n"); break;
        case TOKEN_END:           printf("End\n"); break;
        case TOKEN_NEWLINE:       printf("Newline\n"); break;
        case TOKEN_BYTE:
            printf("Byte: %"PRId8"\n", *((adept_byte*) tokenlist->tokens[i].data));
            break;
        case TOKEN_UBYTE:
            printf("Unsigned Byte: %"PRIu8"\n", *((adept_ubyte*) tokenlist->tokens[i].data));
            break;
        case TOKEN_SHORT:
            printf("Short: %"PRId16"\n", *((adept_short*) tokenlist->tokens[i].data));
            break;
        case TOKEN_USHORT:
            printf("Unsigned Short: %"PRIu16"\n", *((adept_ushort*) tokenlist->tokens[i].data));
            break;
        case TOKEN_INT:
            printf("Integer: %"PRId32"\n", *((adept_int*) tokenlist->tokens[i].data));
            break;
        case TOKEN_UINT:
            printf("Unsigned Integer: %"PRIu32"\n", *((adept_uint*) tokenlist->tokens[i].data));
            break;
        case TOKEN_LONG:
            printf("Long: %"PRId64"\n", *((adept_long*) tokenlist->tokens[i].data));
            break;
        case TOKEN_ULONG:
            printf("Unsigned Long: %"PRIu64"\n", *((adept_ulong*) tokenlist->tokens[i].data));
            break;
        case TOKEN_USIZE:
            printf("Unsigned Long: %"PRIu64"\n", *((adept_usize*) tokenlist->tokens[i].data));
            break;
        case TOKEN_FLOAT:
            printf("Float: %f\n", (double) *((adept_float*) tokenlist->tokens[i].data));
            break;
        case TOKEN_DOUBLE:
            printf("Double: %f\n", *((adept_double*) tokenlist->tokens[i].data));
            break;
        case TOKEN_MEMBER:            printf("Member\n"); break;
        case TOKEN_ADDRESS:           printf("Address\n"); break;
        case TOKEN_NEXT:              printf("Next\n"); break;
        case TOKEN_BRACKET_OPEN:      printf("Square Bracket Open\n"); break;
        case TOKEN_BRACKET_CLOSE:     printf("Square Bracket Cpen\n"); break;
        case TOKEN_MODULUS:           printf("Modulus\n"); break;
        case TOKEN_GENERIC_INT:       printf("Generic Integer Value\n"); break;
        case TOKEN_GENERIC_FLOAT:     printf("Generic Float Value\n"); break;
        case TOKEN_ADD_ASSIGN:        printf("Addition Assignment\n"); break;
        case TOKEN_SUBTRACT_ASSIGN:   printf("Subtraction Assignment\n"); break;
        case TOKEN_MULTIPLY_ASSIGN:   printf("Multiplication Assignment\n"); break;
        case TOKEN_DIVIDE_ASSIGN:     printf("Division Assignment\n"); break;
        case TOKEN_MODULUS_ASSIGN:    printf("Modulus Assignment\n"); break;
        case TOKEN_BIT_AND_ASSIGN:    printf("Bitwise And Assignment\n"); break;
        case TOKEN_BIT_OR_ASSIGN:     printf("Bitwise Or Assignment\n"); break;
        case TOKEN_BIT_XOR_ASSIGN:    printf("Bitwise Xor Assignment\n"); break;
        case TOKEN_BIT_LS_ASSIGN:     printf("Bitwise Left Shift Assignment\n"); break;
        case TOKEN_BIT_RS_ASSIGN:     printf("Bitwise Right Shift Assignment\n"); break;
        case TOKEN_BIT_LGC_LS_ASSIGN: printf("Bitwise Logical Left Shift Assignment\n"); break;
        case TOKEN_BIT_LGC_RS_ASSIGN: printf("Bitwise Logical Right Shift Assignment\n"); break;
        case TOKEN_ELLIPSIS:          printf("Ellipsis\n"); break;
        case TOKEN_UBERAND:           printf("Uber and\n"); break;
        case TOKEN_UBEROR:            printf("Uber or\n"); break;
        case TOKEN_TERMINATE_JOIN:    printf("Terminate Join\n"); break;
        case TOKEN_COLON:             printf("Colon\n"); break;
        case TOKEN_POD:               printf("Keyword: POD\n"); break;
        case TOKEN_ALIAS:             printf("Keyword: alias\n"); break;
        case TOKEN_AND:               printf("Keyword: and\n"); break;
        case TOKEN_AS:                printf("Keyword: as\n"); break;
        case TOKEN_AT:                printf("Keyword: at\n"); break;
        case TOKEN_BREAK:             printf("Keyword: break\n"); break;
        case TOKEN_CASE:              printf("Keyword: case\n"); break;
        case TOKEN_CAST:              printf("Keyword: cast\n"); break;
        case TOKEN_CONTINUE:          printf("Keyword: continue\n"); break;
        case TOKEN_DEF:               printf("Keyword: def\n"); break;
        case TOKEN_DEFAULT:           printf("Keyword: default\n"); break;
        case TOKEN_DEFER:             printf("Keyword: defer\n"); break;
        case TOKEN_DELETE:            printf("Keyword: delete\n"); break;
        case TOKEN_ELSE:              printf("Keyword: else\n"); break;
        case TOKEN_ENUM:              printf("Keyword: enum\n"); break;
        case TOKEN_EXHAUSTIVE:        printf("Keyword: exhaustive\n"); break;
        case TOKEN_EXTERNAL:          printf("Keyword: external\n"); break;
        case TOKEN_FALLTHROUGH:       printf("Keyword: fallthrough\n"); break;
        case TOKEN_FALSE:             printf("Keyword: false\n"); break;
        case TOKEN_FOR:               printf("Keyword: for\n"); break;
        case TOKEN_FOREIGN:           printf("Keyword: foreign\n"); break;
        case TOKEN_FUNC:              printf("Keyword: func\n"); break;
        case TOKEN_FUNCPTR:           printf("Keyword: funcptr\n"); break;
        case TOKEN_GLOBAL:            printf("Keyword: global\n"); break;
        case TOKEN_IF:                printf("Keyword: if\n"); break;
        case TOKEN_IMPORT:            printf("Keyword: import\n"); break;
        case TOKEN_IN:                printf("Keyword: in\n"); break;
        case TOKEN_INOUT:             printf("Keyword: inout\n"); break;
        case TOKEN_NEW:               printf("Keyword: new\n"); break;
        case TOKEN_NULL:              printf("Keyword: null\n"); break;
        case TOKEN_OR:                printf("Keyword: or\n"); break;
        case TOKEN_OUT:               printf("Keyword: out\n"); break;
        case TOKEN_PACKED:            printf("Keyword: packed\n"); break;
        case TOKEN_PRAGMA:            printf("Keyword: pragma\n"); break;
        case TOKEN_PRIVATE:           printf("Keyword: private\n"); break;
        case TOKEN_PUBLIC:            printf("Keyword: public\n"); break;
        case TOKEN_REPEAT:            printf("Keyword: repeat\n"); break;
        case TOKEN_RETURN:            printf("Keyword: return\n"); break;
        case TOKEN_SIZEOF:            printf("Keyword: sizeof\n"); break;
        case TOKEN_STATIC:            printf("Keyword: sizeof\n"); break;
        case TOKEN_STDCALL:           printf("Keyword: stdcall\n"); break;
        case TOKEN_STRUCT:            printf("Keyword: struct\n"); break;
        case TOKEN_SWITCH:            printf("Keyword: switch\n"); break;
        case TOKEN_THREAD_LOCAL:      printf("Keyword: thread_local\n"); break;
        case TOKEN_TRUE:              printf("Keyword: true\n"); break;
        case TOKEN_TYPEINFO:          printf("Keyword: typeinfo\n"); break;
        case TOKEN_UNDEF:             printf("Keyword: undef\n"); break;
        case TOKEN_UNLESS:            printf("Keyword: unless\n"); break;
        case TOKEN_UNTIL:             printf("Keyword: until\n"); break;
        case TOKEN_VA_ARG:            printf("Keyword: va_arg\n"); break;
        case TOKEN_VA_COPY:           printf("Keyword: va_copy\n"); break;
        case TOKEN_VA_END:            printf("Keyword: va_end\n"); break;
        case TOKEN_VA_START:          printf("Keyword: va_start\n"); break;
        case TOKEN_VERBATIM:          printf("Keyword: verbatim\n"); break;
        case TOKEN_WHILE:             printf("Keyword: while\n"); break;
        case TOKEN_META:
            printf("Meta: \"%s\"\n", (char*) tokenlist->tokens[i].data);
            break;
        case TOKEN_POLYMORPH:
            printf("Polymorph: \"%s\"\n", (char*) tokenlist->tokens[i].data);
            break;
        case TOKEN_MAYBE:          printf("Maybe\n"); break;
        case TOKEN_INCREMENT:      printf("Increment\n"); break;
        case TOKEN_DECREMENT:      printf("Decrement\n"); break;
        case TOKEN_TOGGLE:         printf("Toggle\n"); break;
        default:
            printf("unknown token 0x%08X: THIS TOKEN HAS AN UNKNOWN ID\n", tokenlist->tokens[i].id);
        }
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

const char *global_token_name_table[] = {
    "none",                               // 0x00000000
    "identifier",                         // 0x00000001
    "string literal",                     // 0x00000002
    "null-terminated string",             // 0x00000003
    "add",                                // 0x00000004
    "subtract",                           // 0x00000005
    "multiply",                           // 0x00000006
    "divide",                             // 0x00000007
    "assign",                             // 0x00000008
    "equals",                             // 0x00000009
    "not equal",                          // 0x0000000A
    "less than",                          // 0x0000000B
    "greater than",                       // 0x0000000C
    "less than or equal",                 // 0x0000000D
    "greater than or equal",              // 0x0000000E
    "not",                                // 0x0000000F
    "opening parenthesis",                // 0x00000010
    "closing parenthesis",                // 0x00000011
    "beginning brace",                    // 0x00000012
    "ending brace",                       // 0x00000013
    "newline",                            // 0x00000014
    "byte literal",                       // 0x00000015
    "ubyte literal",                      // 0x00000016
    "short literal",                      // 0x00000017
    "ushort literal",                     // 0x00000018
    "int literal",                        // 0x00000019
    "uint literal",                       // 0x0000001A
    "long literal",                       // 0x0000001B
    "ulong literal",                      // 0x0000001C
    "usize literal",                      // 0x0000001D
    "float literal",                      // 0x0000001E
    "double literal",                     // 0x0000001F
    "member access operator",             // 0x00000021
    "address operator",                   // 0x00000020
    "next operator",                      // 0x00000022
    "square bracket open",                // 0x00000023
    "square bracket close",               // 0x00000024
    "modulus",                            // 0x00000025
    "generic integer value",              // 0x00000026
    "generic float value",                // 0x00000027
    "add assign",                         // 0x00000028
    "subtract assign",                    // 0x00000029
    "multiply assign",                    // 0x0000002A
    "divide assign",                      // 0x0000002B
    "modulus assign",                     // 0x0000002C
    "bitwise-and assign",                 // 0x0000002D
    "bitwise-or assign",                  // 0x0000002E
    "bitwise-xor assign",                 // 0x0000002F
    "bitwise left shift assign",          // 0x00000030
    "bitwise right shift assign",         // 0x00000031
    "bitwise logical left shift assign",  // 0x00000032
    "bitwise logical right shift assign", // 0x00000033
    "ellipsis",                           // 0x00000034
    "uber and",                           // 0x00000035
    "uber or",                            // 0x00000036
    "terminate join",                     // 0x00000037
    "colon",                              // 0x00000038
    "bit or",                             // 0x00000039
    "bit and",                            // 0x0000003A
    "left shift",                         // 0x0000003B
    "right shift",                        // 0x0000003C
    "complement",                         // 0x0000003D
    "logical left shift",                 // 0x0000003E
    "logical right shift",                // 0x0000003F
    "namespace",                          // 0x00000040
    "meta directive",                     // 0x00000041
    "polymorph",                          // 0x00000042
    "maybe",                              // 0x00000043
    "increment",                          // 0x00000044
    "decrement",                          // 0x00000045
    "toggle",                             // 0x00000046
    "reserved",                           // 0x00000047
    "reserved",                           // 0x00000048
    "reserved",                           // 0x00000049
    "reserved",                           // 0x0000004A
    "reserved",                           // 0x0000004B
    "reserved",                           // 0x0000004C
    "reserved",                           // 0x0000004D
    "reserved",                           // 0x0000004E
    "reserved",                           // 0x0000004F
    "POD keyword",                        // 0x00000050
    "alias keyword",                      // 0x00000051
    "and keyword",                        // 0x00000052
    "as keyword",                         // 0x00000053
    "at keyword",                         // 0x00000054
    "break keyword",                      // 0x00000055
    "case keyword",                       // 0x00000056
    "cast keyword",                       // 0x00000057
    "continue keyword",                   // 0x00000058
    "def keyword",                        // 0x00000059
    "default keyword",                    // 0x0000005A
    "defer keyword",                      // 0x0000005B
    "delete keyword",                     // 0x0000005C
    "each keyword",                       // 0x0000005D
    "else keyword",                       // 0x0000005E
    "enum keyword",                       // 0x0000005F
    "exhaustive keyword",                 // 0x00000060
    "external keyword",                   // 0x00000061
    "fallthrough keyword",                // 0x00000062
    "false keyword",                      // 0x00000063
    "for keyword",                        // 0x00000064
    "foreign keyword",                    // 0x00000065
    "func keyword",                       // 0x00000066
    "funcptr keyword",                    // 0x00000067
    "global keyword",                     // 0x00000068
    "if keyword",                         // 0x00000069
    "import keyword",                     // 0x0000006A
    "in keyword",                         // 0x0000006B
    "inout keyword",                      // 0x0000006C
    "new keyword",                        // 0x0000006D
    "null keyword",                       // 0x0000006E
    "or keyword",                         // 0x0000006F
    "out keyword",                        // 0x00000070
    "packed keyword",                     // 0x00000071
    "pragma keyword",                     // 0x00000072
    "private keyword",                    // 0x00000073
    "public keyword",                     // 0x00000074
    "repeat keyword",                     // 0x00000075
    "return keyword",                     // 0x00000076
    "sizeof keyword",                     // 0x00000077
    "static keyword",                     // 0x00000078
    "stdcall keyword",                    // 0x00000079
    "struct keyword",                     // 0x0000007A
    "switch keyword",                     // 0x0000007B
    "thread_local keyword",               // 0x0000007C
    "true keyword",                       // 0x0000007D
    "typeinfo keyword",                   // 0x0000007E
    "undef keyword",                      // 0x0000007F
    "unless keyword",                     // 0x00000080
    "until keyword",                      // 0x00000081
    "va_arg keyword",                     // 0x00000082
    "va_copy keyword",                    // 0x00000083
    "va_end keyword",                     // 0x00000084
    "va_start keyword",                   // 0x00000085
    "verbatim keyword",                   // 0x00000086
    "while keyword",                      // 0x00000087
    "reserved",                           // 0x00000088
    "reserved",                           // 0x00000089
    "reserved",                           // 0x0000008A
    "reserved",                           // 0x0000008B
    "reserved",                           // 0x0000008C
    "reserved",                           // 0x0000008D
    "reserved",                           // 0x0000008E
    "reserved",                           // 0x0000008F
};
