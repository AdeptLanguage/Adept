
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "UTIL/color.h"
#include "UTIL/ground.h"
// Include implementation of jsmn
#include "UTIL/jsmn.h"
#include "UTIL/jsmn_helper.h"

void jsmnh_obj_ctx_init(
    jsmnh_obj_ctx_t *ctx,
    jsmnh_buffer_t fulltext,
    jsmnh_tokens_t tokens,
    jsmntok_t master_token,
    length_t token_index,
    jsmnh_buffer_t *use_existing_value_buffer,
    bool owns_tokens
){
    // NOTE: Takes ownership of 'tokens' if 'owns_tokens'
    // NOTE: Will never take ownership of '*use_existing_value_buffer'

    ctx->fulltext = fulltext;
    ctx->tokens = tokens;
    ctx->master_token = master_token;
    ctx->token_index = token_index;
    ctx->total_sections = master_token.size;
    ctx->value = use_existing_value_buffer ? *use_existing_value_buffer : (jsmnh_buffer_t){
        .content = malloc(1024),
        .capacity = 1024
    };
    ctx->owns_value_buffer = use_existing_value_buffer == NULL;
    ctx->owns_tokens = owns_tokens;
}

errorcode_t jsmnh_obj_ctx_easy_init(jsmnh_obj_ctx_t *ctx, char *raw_buffer, length_t raw_buffer_length){
    *ctx = (jsmnh_obj_ctx_t){0};

    jsmn_parser parser;
    jsmn_init(&parser);

    // Try to precompute number of tokens needed
    length_t required_tokens = jsmn_parse(&parser, raw_buffer, raw_buffer_length, NULL, 0);
    if(required_tokens < 0) return FAILURE;

    // Preallocate enough space for tokens
    jsmntok_t *raw_tokens = malloc(sizeof(jsmntok_t) * required_tokens);

    // Try to parse JSON
    jsmn_init(&parser);

    // Parse syntax error
    int parse_error = jsmn_parse(&parser, raw_buffer, raw_buffer_length, raw_tokens, required_tokens);

    jsmnh_tokens_t tokens = (jsmnh_tokens_t){
        .tokens = raw_tokens,
        .length = required_tokens,
    };

    if(parse_error < 0
    || (int) required_tokens != parse_error
    || !jsmnh_get_object(raw_buffer, tokens, 0)){
        return FAILURE;
    }

    // Setup values for processing context
    length_t token_start_index = 1;
    jsmntok_t master_token = raw_tokens[0];

    jsmnh_buffer_t fulltext = (jsmnh_buffer_t){
        .content = raw_buffer,
        .capacity = raw_buffer_length,
    };

    // Create helper context for reading JSON object
    jsmnh_obj_ctx_init(ctx, fulltext, tokens, master_token, token_start_index, NULL, true);
    return SUCCESS;
}

void jsmnh_obj_ctx_free(jsmnh_obj_ctx_t *ctx){
    if(ctx->owns_tokens){
        free(ctx->tokens.tokens);
    }

    if(ctx->owns_value_buffer){
        free(ctx->value.content);
    }
}

errorcode_t jsmnh_obj_ctx_subobject(jsmnh_obj_ctx_t *ctx, jsmnh_obj_ctx_t *forked_output){
    if(!jsmnh_obj_ctx_get_object(ctx)) return FAILURE;

    jsmntok_t new_master_token = ctx->tokens.tokens[ctx->token_index];
    jsmnh_obj_ctx_init(forked_output, ctx->fulltext, ctx->tokens, new_master_token, ctx->token_index + 1, &ctx->value, false);
    return SUCCESS;
}

errorcode_t jsmnh_obj_ctx_read_key(jsmnh_obj_ctx_t *ctx){
    // NOTE: Returns NULL on failure,
    // NOTE: Will advance 'ctx->token_index' on success

    if(!jsmnh_obj_ctx_get_fixed_string(ctx, ctx->value.content, ctx->value.capacity)){
        return FAILURE;
    }

    ctx->token_index += 1;
    return SUCCESS;
}

bool jsmnh_obj_ctx_eq(jsmnh_obj_ctx_t *ctx, weak_cstr_t string){
    return streq(ctx->value.content, string);
}

void jsmnh_obj_ctx_blind_advance(jsmnh_obj_ctx_t *ctx){
    ctx->token_index += jsmnh_subtoken_count(ctx->tokens.tokens, ctx->token_index);
}


void jsmnh_print_token(weak_cstr_t buffer, jsmntok_t token){
    switch(token.type){
    case JSMN_UNDEFINED: printf("UNDEFINED"); break;
    case JSMN_OBJECT:    printf("OBJECT"); break;
    case JSMN_STRING:    printf("STRING"); break;
    case JSMN_ARRAY:     printf("ARRAY"); break;
    case JSMN_PRIMITIVE: printf("PRIMITIVE"); break;
    default:             printf("UNKNOWN"); break;
    }

    length_t range_length = token.end - token.start;

    if(range_length != 0){
        strong_cstr_t range = malloc(range_length + 1);
        memcpy(range, buffer + token.start, range_length);
        range[range_length] = 0x00;
        printf(", range = \"%s\"\n", range);
        free(range);
    } else {
        printf("\n");
    }
}

void jsmnh_print_tokens(jsmntok_t *tokens, length_t length){
    for(length_t i = 0; i != length; i++){
        switch(tokens[i].type){
        case JSMN_UNDEFINED: printf("%d UNDEFINED %d\n", (int) i, tokens[i].size); break;
        case JSMN_OBJECT:    printf("%d OBJECT %d\n", (int) i, tokens[i].size); break;
        case JSMN_STRING:    printf("%d STRING %d\n", (int) i, tokens[i].size); break;
        case JSMN_ARRAY:     printf("%d ARRAY %d\n", (int) i, tokens[i].size); break;
        case JSMN_PRIMITIVE: printf("%d PRIMITIVE %d\n", (int) i, tokens[i].size); break;
        default:             printf("%d UNKNOWN %d\n", (int) i, tokens[i].size); break;
        }
    }
}

successful_t jsmnh_get_object(char *fulltext, jsmnh_tokens_t tokens, length_t token_index){
    if(token_index >= tokens.length){
        #ifdef JSMN_HELPER_LOG_ERRORS
        internalwarningprintf("jsmnh_get_object() failed, out of tokens\n");
        #endif
        return false;
    }

    if(tokens.tokens[token_index].type != JSMN_OBJECT){
        #ifdef JSMN_HELPER_LOG_ERRORS
        internalwarningprintf("jsmnh_get_object() expected object, got something else\n");
        printf("   Got: ");
        jsmnh_print_token(fulltext, tokens.tokens[token_index]);
        #endif
        return false;
    }

    return true;
}

successful_t jsmnh_obj_ctx_get_object(jsmnh_obj_ctx_t *ctx){
    return jsmnh_get_object(ctx->fulltext.content, ctx->tokens, ctx->token_index);
}

successful_t jsmnh_get_array(char *fulltext, jsmnh_tokens_t tokens, length_t token_index){
    if(token_index >= tokens.length){
        #ifdef JSMN_HELPER_LOG_ERRORS
        internalwarningprintf("jsmnh_get_array() failed, out of tokens\n");
        #endif
        return false;
    }

    if(tokens.tokens[token_index].type != JSMN_ARRAY){
        #ifdef JSMN_HELPER_LOG_ERRORS
        internalwarningprintf("jsmnh_get_array() expected array, got something else\n");
        printf("   Got: ");
        jsmnh_print_token(fulltext, tokens.tokens[token_index]);
        #endif
        return false;
    }

    return true;
}

successful_t jsmnh_obj_ctx_get_array(jsmnh_obj_ctx_t *ctx){
    return jsmnh_get_array(ctx->fulltext.content, ctx->tokens, ctx->token_index);
}

successful_t jsmnh_get_fixed_string(char *fulltext, jsmnh_tokens_t tokens, length_t token_index, char *out_content, length_t max_output_size){
    if(token_index >= tokens.length){
        #ifdef JSMN_HELPER_LOG_ERRORS
        internalwarningprintf("jsmnh_get_string() failed, out of tokens\n");
        #endif
        return false;
    }

    if(tokens.tokens[token_index].type != JSMN_STRING){
        #ifdef JSMN_HELPER_LOG_ERRORS
        internalwarningprintf("jsmnh_get_string() expected string, got something else\n");
        printf("   Got: ");
        jsmnh_print_token(fulltext, tokens.tokens[token_index]);
        #endif
        return false;
    }

    length_t length = tokens.tokens[token_index].end - tokens.tokens[token_index].start;
    if(length >= max_output_size) length = max_output_size - 1;
    memcpy(out_content, fulltext + tokens.tokens[token_index].start, length);
    out_content[length] = 0x00;
    return true;
}

successful_t jsmnh_obj_ctx_get_fixed_string(jsmnh_obj_ctx_t *ctx, char *out_content, length_t max_output_size){
    return jsmnh_get_fixed_string(ctx->fulltext.content, ctx->tokens, ctx->token_index, out_content, max_output_size);
}

successful_t jsmnh_get_variable_string(char *fulltext, jsmnh_tokens_t tokens, length_t token_index, strong_cstr_t *out_content){
    if(token_index >= tokens.length){
        #ifdef JSMN_HELPER_LOG_ERRORS
        internalwarningprintf("jsmnh_get_vstring() failed, out of tokens\n");
        #endif
        return false;
    }

    if(tokens.tokens[token_index].type != JSMN_STRING){
        #ifdef JSMN_HELPER_LOG_ERRORS
        internalwarningprintf("jsmnh_get_vstring() expected string, got something else\n");
        printf("   Got: ");
        jsmnh_print_token(fulltext, tokens.tokens[token_index]);
        #endif
        return false;
    }

    length_t length = tokens.tokens[token_index].end - tokens.tokens[token_index].start;
    *out_content = malloc(length + 1);
    memcpy(*out_content, fulltext + tokens.tokens[token_index].start, length);
    (*out_content)[length] = 0x00;
    return true;
}

successful_t jsmnh_obj_ctx_get_variable_string(jsmnh_obj_ctx_t *ctx, strong_cstr_t *out_content){
    return jsmnh_get_variable_string(ctx->fulltext.content, ctx->tokens, ctx->token_index, out_content);
}

successful_t jsmnh_get_integer(char *fulltext, jsmnh_tokens_t tokens, length_t token_index, long long *out_value){
    if(token_index >= tokens.length){
        #ifdef JSMN_HELPER_LOG_ERRORS
        internalwarningprintf("jsmnh_get_integer() failed, out of tokens\n");
        #endif
        return false;
    }

    if(tokens.tokens[token_index].type != JSMN_PRIMITIVE){
        #ifdef JSMN_HELPER_LOG_ERRORS
        internalwarningprintf("jsmnh_get_integer() expected primitive, got something else\n");
        printf("   Got: ");
        jsmnh_print_token(fulltext, tokens.tokens[token_index]);
        #endif
        return false;
    }

    char first_char = *(fulltext + tokens.tokens[token_index].start);
    if(first_char != '-' && (first_char < '0' || first_char > '9')){
        #ifdef JSMN_HELPER_LOG_ERRORS
        internalwarningprintf("jsmnh_get_integer() expected integer, got a different primitive\n");
        printf("   Got: ");
        jsmnh_print_token(fulltext, tokens.tokens[token_index]);
        #endif
        return false;
    }

    length_t length = tokens.tokens[token_index].end - tokens.tokens[token_index].start;
    if(length >= 128) length = 127;

    char value_string[128];
    memcpy(value_string, fulltext + tokens.tokens[token_index].start, length);
    value_string[length] = 0x00;
    *out_value = atoll(value_string);
    return true;
}

successful_t jsmnh_obj_ctx_get_integer(jsmnh_obj_ctx_t *ctx, long long *out_value){
    return jsmnh_get_integer(ctx->fulltext.content, ctx->tokens, ctx->token_index, out_value);
}

successful_t jsmnh_get_boolean(char *fulltext, jsmnh_tokens_t tokens, length_t token_index, bool *out_bool){
    if(token_index >= tokens.length){
        #ifdef JSMN_HELPER_LOG_ERRORS
        internalwarningprintf("jsmnh_get_boolean() failed, out of tokens\n");
        #endif
        return false;
    }

    if(tokens.tokens[token_index].type != JSMN_PRIMITIVE){
        #ifdef JSMN_HELPER_LOG_ERRORS
        internalwarningprintf("jsmnh_get_boolean() expected primitive, got something else\n");
        printf("   Got: ");
        jsmnh_print_token(fulltext, tokens.tokens[token_index]);
        #endif
        return false;
    }

    char first_char = *(fulltext + tokens.tokens[token_index].start);
    if(first_char != 't' && first_char != 'f'){
        #ifdef JSMN_HELPER_LOG_ERRORS
        internalwarningprintf("jsmnh_get_boolean() expected boolean, got a different primitive\n");
        printf("   Got: ");
        jsmnh_print_token(fulltext, tokens.tokens[token_index]);
        #endif
        return false;
    }

    *out_bool = first_char == 't';
    return true;
}

successful_t jsmnh_obj_ctx_get_boolean(jsmnh_obj_ctx_t *ctx, bool *out_bool){
    return jsmnh_get_boolean(ctx->fulltext.content, ctx->tokens, ctx->token_index, out_bool);
}

length_t jsmnh_subtoken_count(jsmntok_t *tokens, length_t start_index){
    length_t index = start_index;
    jsmntok_t token = tokens[index++];
    switch(token.type){
    case JSMN_UNDEFINED: case JSMN_PRIMITIVE: return 1;
    case JSMN_OBJECT: case JSMN_STRING: case JSMN_ARRAY:
        for(int i = 0; i != token.size; i++){
            index += jsmnh_subtoken_count(tokens, index);
        }
        return index - start_index;
    }
    return 0;
}

weak_cstr_t jsmnh_parse_fail_reason(int code){
    switch(code){
    case JSMN_ERROR_INVAL: return "JSON is corrupted";
    case JSMN_ERROR_NOMEM: return "out of memory";
    case JSMN_ERROR_PART:  return "JSON is incomplete";
    }
    return "UNKNOWN";
}
