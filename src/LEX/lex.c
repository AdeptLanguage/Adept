
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "DRVR/compiler.h"
#include "DRVR/object.h"
#include "LEX/lex.h"
#include "LEX/token.h"
#include "TOKEN/token_data.h"
#include "UTIL/color.h"
#include "UTIL/datatypes.h"
#include "UTIL/filename.h"
#include "UTIL/ground.h"
#include "UTIL/search.h"
#include "UTIL/util.h"

/*
    NOTE: We require the buffer to be terminated with '\n\0'

    This allows us to use a few techniques that otherwise
    would be illegal on normal buffers or C-Strings. 

    Techniques used/abused include:
    - Ability to read two characters ahead w/o checking buffer size - e.g. memcmp(ctx->buffer[ctx->i + 1], "ab", 2) assuming ctx->i is valid and non-newline
    - Ability to skip until a newline w/o checking buffer size - e.g. while(ctx->buffer[ctx->i] != '\n') ctx->i++; assuming ctx->i is valid and non-newline
    - Automatic TOKEN_NEWLINE as last token
*/

typedef struct {
    const char *buffer;
    length_t buffer_length;
    length_t object_index;
    tokenlist_t tokenlist;
    length_t i;
} lex_ctx_t;

static inline void add_token(tokenlist_t *tokenlist, token_t token, source_t source){
    coexpand(
        (void**) &tokenlist->tokens, sizeof(token_t),
        (void**) &tokenlist->sources, sizeof(source_t),
        tokenlist->length, &tokenlist->capacity,
        /* amount */ 1, /* default capacity */ 4
    );

    tokenlist->tokens[tokenlist->length] = token;
    tokenlist->sources[tokenlist->length++] = source;
}

static inline token_t character_to_token(char c){
    tokenid_t id;

    switch(c){
    case '(':  id = TOKEN_OPEN;           break;
    case ')':  id = TOKEN_CLOSE;          break;
    case '{':  id = TOKEN_BEGIN;          break;
    case '}':  id = TOKEN_END;          break;
    case '[':  id = TOKEN_BRACKET_OPEN;   break;
    case ']':  id = TOKEN_BRACKET_CLOSE;  break;
    case ',':  id = TOKEN_NEXT;           break;
    case ';':  id = TOKEN_TERMINATE_JOIN; break;
    case '?':  id = TOKEN_MAYBE;          break;
    case '\n': id = TOKEN_NEWLINE;        break;
    default:   id = TOKEN_NONE;
    }

    return (token_t){id, NULL};
}

static inline void add_simple_token(lex_ctx_t *ctx){
    add_token(&ctx->tokenlist, character_to_token(ctx->buffer[ctx->i]), (source_t){ctx->i, 1, ctx->object_index});
}

static inline void cases(lex_ctx_t *ctx, char cases[], tokenid_t tokenids[], length_t count, tokenid_t tokenid_default){
    for(length_t i = 0; i != count; i++){
        if(ctx->buffer[ctx->i + 1] == cases[i]){
            add_token(&ctx->tokenlist, (token_t){tokenids[i], NULL}, (source_t){ctx->i, 2, ctx->object_index});
            ctx->i += 1;
            return;
        }
    }

    add_token(&ctx->tokenlist, (token_t){tokenid_default, NULL}, (source_t){ctx->i, 1, ctx->object_index});
}

static inline void options(lex_ctx_t *ctx, char **options, tokenid_t *cases, length_t count){
    // Strings supplied to 'options' must not be empty

    for(length_t i = 0; i != count; i++){
        length_t len = strlen(options[i]);

        if(memcmp(&ctx->buffer[ctx->i], options[i], len) == 0){
            add_token(&ctx->tokenlist, (token_t){cases[i], NULL}, (source_t){ctx->i, len, ctx->object_index});
            ctx->i += len - 1;
            return;
        }
    }
}

static inline void stacking(lex_ctx_t *ctx, char character, tokenid_t *cases, length_t count){
    length_t stride = 1;

    while(stride < count && ctx->buffer[ctx->i + stride] == character){
        stride += 1;
    }

    add_token(&ctx->tokenlist, (token_t){cases[stride - 1], NULL}, (source_t){ctx->i, stride, ctx->object_index});
    ctx->i += stride - 1;
}

static inline errorcode_t string(lex_ctx_t *ctx, compiler_t *optional_error_compiler, object_t *optional_error_object){
    const char *beginning = &ctx->buffer[ctx->i + 1];
    const char *end = beginning;
    const char *eof = &ctx->buffer[ctx->buffer_length];

    // Scout out string contents
    while(end < eof && *end != '"'){
        if(*end == '\\'){
            end += 2;
        } else {
            end += 1;
        }
    }

    // Construct string contents and process escapes
    length_t put = 0;
    length_t size = end - beginning;
    char *string = malloc(size + 1);

    for(length_t i = 0; i != size;){
        if(beginning[i] != '\\'){
            string[put++] = beginning[i++];
            continue;
        }

        switch(beginning[i + 1]){
        case 'n': string[put++] = '\n';  break;
        case 'r': string[put++] = '\r';  break;
        case 't': string[put++] = '\t';  break;
        case 'b': string[put++] = '\b';  break;
        case '0': string[put++] = '\0';  break;
        case '"': string[put++] = '"';   break;
        case 'e': string[put++] = 0x1B;  break;
        case '\'': string[put++] = '\''; break;
        case '\\': string[put++] = '\\'; break;
        default:
            if(optional_error_compiler && optional_error_object){
                int line, column;
                lex_get_location(ctx->buffer, ctx->i + i, &line, &column);
                redprintf("%s:%d:%d: Unknown escape sequence '\\%c'\n", filename_name_const(optional_error_object->filename), line, column, ctx->buffer[ctx->i + i]);
                compiler_print_source(optional_error_compiler, line, (source_t){ctx->i, 2, ctx->object_index});
            }
            return FAILURE;
        }

        i += 2;
    }

    string[put] = '\0';

    // Create string data container
    token_string_data_t *string_data = malloc(sizeof(token_string_data_t));
    *string_data = (token_string_data_t){
        .array = string,
        .length = put
    };

    // Create string token
    add_token(&ctx->tokenlist, (token_t){TOKEN_STRING, string_data}, (source_t){ctx->i, size + 2, ctx->object_index});
    ctx->i += size + 1;
    return SUCCESS;
}

static inline errorcode_t cstring(lex_ctx_t *ctx, compiler_t *optional_error_compiler, object_t *optional_error_object){
    const char *beginning = &ctx->buffer[ctx->i + 1];
    const char *end = beginning;
    const char *eof = ctx->buffer + ctx->buffer_length;

    // Scout out string contents
    while(end < eof && *end != '\''){
        if(*end == '\\'){
            end += 2;
        } else {
            end += 1;
        }
    }

    // Construct string contents and process escapes
    length_t put = 0;
    length_t size = end - beginning;
    char *string = malloc(size + 1);

    for(length_t i = 0; i != size;){
        if(beginning[i] != '\\'){
            string[put++] = beginning[i++];
            continue;
        }

        switch(beginning[i + 1]){
        case 'n': string[put++] = '\n';  break;
        case 'r': string[put++] = '\r';  break;
        case 't': string[put++] = '\t';  break;
        case 'b': string[put++] = '\b';  break;
        case '0': string[put++] = '\0';  break;
        case '"': string[put++] = '"';   break;
        case 'e': string[put++] = 0x1B;  break;
        case '\'': string[put++] = '\''; break;
        case '\\': string[put++] = '\\'; break;
        default:
            if(optional_error_compiler && optional_error_object){
                int line, column;
                lex_get_location(ctx->buffer, ctx->i + i, &line, &column);
                redprintf("%s:%d:%d: Unknown escape sequence '\\%c'\n", filename_name_const(optional_error_object->filename), line, column, ctx->buffer[ctx->i + i]);
                compiler_print_source(optional_error_compiler, line, (source_t){ctx->i, 2, ctx->object_index});
            }
            return FAILURE;
        }

        i += 2;
    }

    // Check if this is actually a character literal
    if(put == 1){
        if(memcmp(&ctx->buffer[ctx->i + size + 2], "ub", 2) == 0){
            // Actually a 'ubyte' character literal
            add_token(&ctx->tokenlist, (token_t){TOKEN_UBYTE, (unsigned char*) string}, (source_t){ctx->i, size + 4, ctx->object_index});
            ctx->i += size + 3;
            return SUCCESS;
        }

        if(memcmp(&ctx->buffer[ctx->i + size + 2], "sb", 2) == 0){
            // Actually a 'byte' character literal
            add_token(&ctx->tokenlist, (token_t){TOKEN_BYTE, (char*) string}, (source_t){ctx->i, size + 4, ctx->object_index});
            ctx->i += size + 3;
            return SUCCESS;
        }
    }

    // Otherwise, this lexical construct is a C-String
    string[put] = '\0';

    // Create C-String token
    add_token(&ctx->tokenlist, (token_t){TOKEN_CSTRING, string}, (source_t){ctx->i, size + 2, ctx->object_index});
    ctx->i += size + 1;
    return SUCCESS;
}

static inline errorcode_t number(lex_ctx_t *ctx, compiler_t *optional_error_compiler, object_t *optional_error_object){
    const char *beginning = &ctx->buffer[ctx->i];
    const char *end = beginning;
    const int buf_size = 96;
    bool is_hex = false;
    bool can_dot = true;
    bool did_exp = false;

    char buf[buf_size];
    int put = 0;

    if(*end == '-'){
        // Negative number
        buf[put++] = *(end++);
    } else if(memcmp(beginning, "0x", 2) == 0 || memcmp(beginning, "0X", 2) == 0){
        // Hexadecimal number
        is_hex = true;
        can_dot = false;
        end += 2;
    }

    while(put < buf_size){
        if(isdigit(*end) || *end == '_'){
            buf[put++] = *(end++);
        } else if(*end == '.' && can_dot){
            buf[put++] = *(end++);
            can_dot = false;
        } else if(is_hex && ((*end >= 'A' && *end <= 'F') || (*end >= 'a' && *end <= 'f'))){
            buf[put++] = *(end++);
        } else if(!did_exp && (*end == 'e' || *end == 'E')){
            buf[put++] = *(end++);
            did_exp = true;
            can_dot = false;

            if(*end == '+' || *end == '-'){
                buf[put++] = *(end++);
            }
        } else {
            break;
        }
    }

    if(put >= buf_size){
        if(optional_error_compiler && optional_error_object){
            int line, column;
            lex_get_location(ctx->buffer, ctx->i, &line, &column);
            redprintf("%s:%d:%d: Number is too long (%d characters max)\n", filename_name_const(optional_error_object->filename), line, column, buf_size - 1);
            compiler_print_source(optional_error_compiler, line, (source_t){ctx->i, buf_size - 1, ctx->object_index});
        }
        return FAILURE;
    }

    buf[put] = '\0';

    int base = is_hex ? 16 : 10;
    tokenid_t token_id;
    void *data;
    length_t stride = end - beginning;

    // Respect integer/float suffixes
    switch(*end){
    case 'u':
        switch(*(end + 1)){
        case 'b':
            token_id = TOKEN_UBYTE;
            data = malloc(sizeof(adept_ubyte));
            *((adept_ubyte*) data) = string_to_uint8(buf, base);
            stride += 2;
            break;
        case 's':
            token_id = TOKEN_USHORT;
            data = malloc(sizeof(adept_ushort));
            *((adept_ushort*) data) = string_to_uint16(buf, base);
            stride += 2;
            break;
        case 'i':
            token_id = TOKEN_UINT;
            data = malloc(sizeof(adept_uint));
            *((adept_uint*) data) = string_to_uint32(buf, base);
            stride += 2;
            break;
        case 'l':
            token_id = TOKEN_ULONG;
            data = malloc(sizeof(adept_ulong));
            *((adept_ulong*) data) = string_to_uint64(buf, base);
            stride += 2;
            break;
        case 'z':
            token_id = TOKEN_USIZE;
            data = malloc(sizeof(adept_usize));
            *((adept_usize*) data) = string_to_uint64(buf, base);
            stride += 2;
            break;
        default: {
                int line, column;
                lex_get_location(ctx->buffer, ctx->i + (end - beginning + 1), &line, &column);
                redprintf("%s:%d:%d: Expected valid number suffix after 'u' base suffix\n", filename_name_const(optional_error_object->filename), line, column);
                return FAILURE;
            }
        }
        break;
    case 's':
        switch(*(end + 1)){
        case 'b':
            token_id = TOKEN_BYTE;
            data = malloc(sizeof(adept_byte));
            *((adept_byte*) data) = string_to_int8(buf, base);
            stride += 2;
            break;
        case 's':
            token_id = TOKEN_SHORT;
            data = malloc(sizeof(adept_ushort));
            *((short*) data) = string_to_int16(buf, base);
            stride += 2;
            break;
        case 'i':
            token_id = TOKEN_INT;
            data = malloc(sizeof(adept_int));
            *((adept_int*) data) = string_to_int32(buf, base);
            stride += 2;
            break;
        case 'l':
            token_id = TOKEN_LONG;
            data = malloc(sizeof(adept_long));
            *((adept_long*) data) = string_to_int64(buf, base);
            stride += 2;
            break;
        default:
            token_id = TOKEN_SHORT;
            data = malloc(sizeof(adept_short));
            *((adept_short*) data) = string_to_int16(buf, base);
            stride += 1;
        }
        break;
    case 'b':
        token_id = TOKEN_BYTE;
        data = malloc(sizeof(adept_byte));
        *((adept_byte*) data) = string_to_int8(buf, base);
        stride += 1;
        break;
    case 'i':
        token_id = TOKEN_INT;
        data = malloc(sizeof(adept_int));
        *((adept_int*) data) = string_to_int32(buf, base);
        stride += 1;
        break;
    case 'l':
        token_id = TOKEN_LONG;
        data = malloc(sizeof(adept_long));
        *((adept_long*) data) = string_to_int64(buf, base);
        stride += 1;
        break;
    case 'f':
        token_id = TOKEN_FLOAT;
        data = malloc(sizeof(adept_float));
        *((adept_float*) data) = string_to_float32(buf);
        stride += 1;
        break;
    case 'd':
        token_id = TOKEN_DOUBLE;
        data = malloc(sizeof(adept_double));
        *((adept_double*) data) = string_to_float64(buf);
        stride += 1;
        break;
    default:
        if((!is_hex && !can_dot) || did_exp){
            // Default to normal generic floating-point
            token_id = TOKEN_GENERIC_FLOAT;
            data = malloc(sizeof(adept_generic_float));
            *((adept_generic_float*) data) = string_to_float64(buf);
        } else if(string_to_int_must_be_uint64(buf, put, base)){
            // Numbers that cannot be expressed using int64 will be promoted to uint64
            token_id = TOKEN_ULONG;
            data = malloc(sizeof(adept_ulong));
            *((adept_ulong*) data) = string_to_uint64(buf, base);
        } else {
            // Otherwise, default to normal generic integer
            token_id = TOKEN_GENERIC_INT;
            data = malloc(sizeof(adept_generic_int));
            *((adept_generic_int*) data) = string_to_int64(buf, base);
        }
    }
    
    // Add number token
    add_token(&ctx->tokenlist, (token_t){token_id, data}, (source_t){ctx->i, stride, ctx->object_index});
    ctx->i += stride - 1;
    return SUCCESS;
}

static inline void running(lex_ctx_t *ctx, tokenid_t intent){
    // Contains additional logic for intents:
    // - TOKEN_WORD
    // - TOKEN_POLYMORPH

    length_t flag_length = intent == TOKEN_WORD ? 0 : 1;
    const char *beginning = &ctx->buffer[ctx->i + flag_length];
    const char *end = beginning;
    const char *eof = ctx->buffer + ctx->buffer_length;

    while(end < eof){
        char c = *end;

        if(c == '_' || isalnum(c)){
            end++;
            continue;
        }

        if(intent == TOKEN_WORD){
            if(c == '\\' || (c == ':' && (isalnum(end[1]) || c == '_'))){
                end++;
                continue;
            }
        }

        if(intent == TOKEN_POLYMORPH && beginning == end){
            if(c == '~'){
                end++;
                continue;
            }

            if(c == '#'){
                intent = TOKEN_POLYCOUNT;
                beginning++;
                end++;
                flag_length++;
                continue;
            }
        }

        break;
    }

    // Calculate size
    length_t size = end - beginning;

    // Create heap-allocated string to hold identifier
    char *identifier = memcpy(malloc(size + 1), beginning, size);
    identifier[size] = '\0';

    if(intent == TOKEN_WORD){
        maybe_index_t keyword_index = binary_string_search(global_token_keywords_list, global_token_keywords_list_length, identifier);
        
        // Handle word tokens that should be keywords
        if(keyword_index != -1){
            add_token(&ctx->tokenlist, (token_t){BEGINNING_OF_KEYWORD_TOKENS + (unsigned int) keyword_index, NULL}, (source_t){ctx->i, size, ctx->object_index});
            ctx->i += size - 1;
            free(identifier);
            return;
        } else if(size == 4 && memcmp(beginning, "elif", 4) == 0){
            // Legacy alternative syntax 'elif'
            add_token(&ctx->tokenlist, (token_t){TOKEN_ELSE, NULL}, (source_t){ctx->i, 2, ctx->object_index});
            add_token(&ctx->tokenlist, (token_t){TOKEN_IF, NULL}, (source_t){ctx->i + 2, 2, ctx->object_index});
            ctx->i += 3;
            free(identifier);
            return;
        }

        // Otherwise not a keyword...

        // Legacy alternative syntax ':' instead of '\\' as a namespace character
        // This will be removed in the future
        for(char *s = identifier; *s; s++){
            if(*s == ':') *s = '\\';
        }
    }

    // Create token
    add_token(&ctx->tokenlist, (token_t){intent, identifier}, (source_t){ctx->i, size + flag_length, ctx->object_index});
    ctx->i += size + flag_length - 1;
}

errorcode_t lex(compiler_t *compiler, object_t *object){
    if(!file_text_contents(object->filename, &object->buffer, &object->buffer_length, true)){
        redprintf("The file '%s' doesn't exist or can't be accessed\n", object->filename);
        return FAILURE;
    }

    return lex_buffer(compiler, object);
}

errorcode_t lex_buffer(compiler_t *compiler, object_t *object){
    // REQUIREMENT: The attached buffer 'object->buffer' must be terminated with '\n\0'
    //     (where \0 is not included in the 'object->buffer_size')

    const char *buffer = object->buffer;
    length_t buffer_length = object->buffer_length;
    length_t estimate = buffer_length / 3;

    lex_ctx_t ctx = (lex_ctx_t){
        .buffer = buffer,
        .buffer_length = buffer_length,
        .object_index = object->index,
        .tokenlist = (tokenlist_t){
            .tokens = malloc(sizeof(token_t) * estimate),
            .length = 0,
            .capacity = estimate,
            .sources = malloc(sizeof(source_t) * estimate),
        },
        .i = 0
    };

    for(; ctx.i != buffer_length; ctx.i++){
        switch(buffer[ctx.i]){
        case ' ':
        case '\t':
            break;
        case '(': case ')':
        case '{': case '}':
        case '[': case ']':
        case ',':
        case ';':
        case '?':
        case '\n':
            add_simple_token(&ctx);
            break;
        case '-':
            if(isdigit(buffer[ctx.i + 1])){
                if(number(&ctx, compiler, object)) goto failure;
            } else {
                cases(&ctx, (char[]){'=', '-'}, (tokenid_t[]){TOKEN_SUBTRACT_ASSIGN, TOKEN_DECREMENT}, 2, TOKEN_SUBTRACT);
            }
            break;
        case '/':
            switch(buffer[ctx.i + 1]){
            case '/':
                do ctx.i += 1; while(buffer[ctx.i + 1] != '\n');
                break;
            case '*': {
                    const char *end = &buffer[ctx.i];
                    const char *eof = &buffer[buffer_length];

                    while(end < eof && memcmp(end, "*/", 2) != 0){
                        end++;
                    }

                    if(end >= eof){
                        int line, column;
                        lex_get_location(buffer, ctx.i, &line, &column);
                        redprintf("%s:%d:%d: Unterminated multiline comment\n", filename_name_const(object->filename), line, column);
                    } else {
                        ctx.i += end - &buffer[ctx.i] + 1;
                    }
                }
                break;
            default:
                cases(&ctx, (char[]){'='}, (tokenid_t[]){TOKEN_DIVIDE_ASSIGN}, 1, TOKEN_DIVIDE);
            }
            break;
        case '<':
            options(
                &ctx, (char*[]){"<<<=", "<<<", "<<=", "<<", "<=", "<"},
                (tokenid_t[]){TOKEN_BIT_LGC_LSHIFT_ASSIGN, TOKEN_BIT_LGC_LSHIFT, TOKEN_BIT_LSHIFT_ASSIGN, TOKEN_BIT_LSHIFT, TOKEN_LESSTHANEQ, TOKEN_LESSTHAN},
                6
            );
            break;
        case '>':
            options(
                &ctx, (char*[]){">>>=", ">>>", ">>=", ">>", ">=", ">"},
                (tokenid_t[]){TOKEN_BIT_LGC_RSHIFT_ASSIGN, TOKEN_BIT_LGC_RSHIFT, TOKEN_BIT_RSHIFT_ASSIGN, TOKEN_BIT_RSHIFT, TOKEN_GREATERTHANEQ, TOKEN_GREATERTHAN},
                6
            );
            break;
        case '=':
            cases(&ctx, (char[]){'=', '>'}, (tokenid_t[]){TOKEN_EQUALS, TOKEN_STRONG_ARROW}, 2, TOKEN_ASSIGN);
            break;
        case '!':
            cases(&ctx, (char[]){'=', '!'}, (tokenid_t[]){TOKEN_NOTEQUALS, TOKEN_TOGGLE}, 2, TOKEN_NOT);
            break;
        case ':':
            cases(&ctx, (char[]){':'}, (tokenid_t[]){TOKEN_ASSOCIATE}, 1, TOKEN_COLON);
            break;
        case '+':
            cases(&ctx, (char[]){'=', '+'}, (tokenid_t[]){TOKEN_ADD_ASSIGN, TOKEN_INCREMENT}, 2, TOKEN_ADD);
            break;
        case '*':
            cases(&ctx, (char[]){'='}, (tokenid_t[]){TOKEN_MULTIPLY_ASSIGN}, 1, TOKEN_MULTIPLY);
            break;
        case '%':
            cases(&ctx, (char[]){'='}, (tokenid_t[]){TOKEN_MODULUS_ASSIGN}, 1, TOKEN_MODULUS);
            break;
        case '^':
            cases(&ctx, (char[]){'='}, (tokenid_t[]){TOKEN_BIT_XOR_ASSIGN}, 1, TOKEN_BIT_XOR);
            break;
        case '~':
            cases(&ctx, (char[]){'>'}, (tokenid_t[]){TOKEN_GIVES}, 1, TOKEN_BIT_COMPLEMENT);
            break;
        case '&':
            cases(&ctx, (char[]){'&', '='}, (tokenid_t[]){TOKEN_UBERAND, TOKEN_BIT_AND_ASSIGN}, 2, TOKEN_BIT_AND /*aka TOKEN_ADDRESS*/);
            break;
        case '|':
            cases(&ctx, (char[]){'|', '='}, (tokenid_t[]){TOKEN_UBEROR, TOKEN_BIT_OR_ASSIGN}, 2, TOKEN_BIT_OR);
            break;
        case '.':
            stacking(&ctx, '.', (tokenid_t[]){TOKEN_MEMBER, TOKEN_RANGE, TOKEN_ELLIPSIS}, 3);
            break;
        case '"':
            if(string(&ctx, compiler, object)) goto failure;
            break;
        case '\'':
            if(cstring(&ctx, compiler, object)) goto failure;
            break;
        case '#':
            running(&ctx, TOKEN_META);
            break;
        case '$':
            running(&ctx, TOKEN_POLYMORPH);
            break;
        default: {
                char c = buffer[ctx.i];

                if(isalpha(c) || c == '_' || c == '\\'){
                    running(&ctx, TOKEN_WORD);
                    break;
                }

                if(isdigit(c)){
                    if(number(&ctx, compiler, object)) goto failure;
                    break;
                }

                int line, column;
                lex_get_location(buffer, ctx.i, &line, &column);
                redprintf("%s:%d:%d: Unrecognized symbol '%c' (0x%02X)\n", filename_name_const(object->filename), line, column, buffer[ctx.i], (int) buffer[ctx.i]);
                compiler_print_source(compiler, line, (source_t){ctx.i, 0, ctx.object_index});
                goto failure;
            }
        }
    }

    object->compilation_stage = COMPILATION_STAGE_TOKENLIST;
    object->tokenlist = ctx.tokenlist;
    return SUCCESS;

failure:
    tokenlist_free(&ctx.tokenlist);
    return FAILURE;
}

void lex_get_location(const char *buffer, length_t index, int *line, int *column){
    // NOTE: Expects index to be pointed at the character that caused the error or is the area of interest

    int newlines = 0;
    const char *last_newline = NULL;

    for(length_t i = 0; i != index; i++){
        if(buffer[i] == '\n'){
            last_newline = &buffer[i];
            newlines++;
        }
    }

    *line = 1 + newlines;
    *column = last_newline ? (int)(&buffer[index] - last_newline) : (int) index + 1;
}
