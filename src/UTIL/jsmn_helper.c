
#include "UTIL/color.h"
#include "UTIL/jsmn_helper.h"

void jsmn_helper_print_token(weak_cstr_t buffer, jsmntok_t token){
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
        free(range);
        printf(", range = \"%s\"\n", range);
    } else {
        printf("\n");
    }
}

void jsmn_helper_print_tokens(jsmntok_t *tokens, length_t length){
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

successful_t jsmn_helper_get_object(weak_cstr_t buffer, jsmntok_t *tokens, length_t num_tokens, length_t index){
    if(index >= num_tokens){
        #ifdef JSMN_HELPER_LOG_ERRORS
        internalwarningprintf("jsmn_helper_get_object() failed, out of tokens\n");
        #endif
        return false;
    }

    if(tokens[index].type != JSMN_OBJECT){
        #ifdef JSMN_HELPER_LOG_ERRORS
        internalwarningprintf("jsmn_helper_get_object() expected object, got something else\n");
        printf("   Got: ");
        jsmn_helper_print_token(buffer, tokens[index]);
        #endif
        return false;
    }

    return true;
}

successful_t jsmn_helper_get_array(weak_cstr_t buffer, jsmntok_t *tokens, length_t num_tokens, length_t index){
    if(index >= num_tokens){
        #ifdef JSMN_HELPER_LOG_ERRORS
        internalwarningprintf("jsmn_helper_get_array() failed, out of tokens\n");
        #endif
        return false;
    }

    if(tokens[index].type != JSMN_OBJECT){
        #ifdef JSMN_HELPER_LOG_ERRORS
        internalwarningprintf("jsmn_helper_get_array() expected array, got something else\n");
        printf("   Got: ");
        jsmn_helper_print_token(buffer, tokens[index]);
        #endif
        return false;
    }

    return true;
}

successful_t jsmn_helper_get_string(weak_cstr_t buffer, jsmntok_t *tokens, length_t num_tokens, length_t index, char *out_content, length_t max_output_size){
    if(index >= num_tokens){
        #ifdef JSMN_HELPER_LOG_ERRORS
        internalwarningprintf("jsmn_helper_get_string() failed, out of tokens\n");
        #endif
        return false;
    }

    if(tokens[index].type != JSMN_STRING){
        #ifdef JSMN_HELPER_LOG_ERRORS
        internalwarningprintf("jsmn_helper_get_string() expected string, got something else\n");
        printf("   Got: ");
        jsmn_helper_print_token(buffer, tokens[index]);
        #endif
        return false;
    }

    length_t length = tokens[index].end - tokens[index].start;
    if(length >= max_output_size) length = max_output_size - 1;
    memcpy(out_content, buffer + tokens[index].start, length);
    out_content[length] = 0x00;
    return true;
}

successful_t jsmn_helper_get_vstring(weak_cstr_t buffer, jsmntok_t *tokens, length_t num_tokens, length_t index, strong_cstr_t *out_content){
    if(index >= num_tokens){
        #ifdef JSMN_HELPER_LOG_ERRORS
        internalwarningprintf("jsmn_helper_get_vstring() failed, out of tokens\n");
        #endif
        return false;
    }

    if(tokens[index].type != JSMN_STRING){
        #ifdef JSMN_HELPER_LOG_ERRORS
        internalwarningprintf("jsmn_helper_get_vstring() expected string, got something else\n");
        printf("   Got: ");
        jsmn_helper_print_token(buffer, tokens[index]);
        #endif
        return false;
    }

    length_t length = tokens[index].end - tokens[index].start;
    *out_content = malloc(length + 1);
    memcpy(*out_content, buffer + tokens[index].start, length);
    (*out_content)[length] = 0x00;
    return true;
}

successful_t jsmn_helper_get_integer(weak_cstr_t buffer, jsmntok_t *tokens, length_t num_tokens, length_t index, long long *out_value){
    if(index >= num_tokens){
        #ifdef JSMN_HELPER_LOG_ERRORS
        internalwarningprintf("jsmn_helper_get_integer() failed, out of tokens\n");
        #endif
        return false;
    }

    if(tokens[index].type != JSMN_PRIMITIVE){
        #ifdef JSMN_HELPER_LOG_ERRORS
        internalwarningprintf("jsmn_helper_get_integer() expected primitive, got something else\n");
        printf("   Got: ");
        jsmn_helper_print_token(buffer, tokens[index]);
        #endif
        return false;
    }

    char first_char = *(buffer + tokens[index].start);
    if(first_char != '-' && (first_char < '0' || first_char > '9')){
        #ifdef JSMN_HELPER_LOG_ERRORS
        internalwarningprintf("jsmn_helper_get_integer() expected integer, got a different primitive\n");
        printf("   Got: ");
        jsmn_helper_print_token(buffer, tokens[index]);
        #endif
        return false;
    }

    length_t length = tokens[index].end - tokens[index].start;
    if(length >= 128) length = 127;

    char value_string[128];
    memcpy(value_string, buffer + tokens[index].start, length);
    value_string[length] = 0x00;
    *out_value = atoll(value_string);
    return true;
}

successful_t jsmn_helper_get_boolean(weak_cstr_t buffer, jsmntok_t *tokens, length_t num_tokens, length_t index, bool *out_value){
    if(index >= num_tokens){
        #ifdef JSMN_HELPER_LOG_ERRORS
        internalwarningprintf("jsmn_helper_get_boolean() failed, out of tokens\n");
        #endif
        return false;
    }

    if(tokens[index].type != JSMN_PRIMITIVE){
        #ifdef JSMN_HELPER_LOG_ERRORS
        internalwarningprintf("jsmn_helper_get_boolean() expected primitive, got something else\n");
        printf("   Got: ");
        jsmn_helper_print_token(buffer, tokens[index]);
        #endif
        return false;
    }

    char first_char = *(buffer + tokens[index].start);
    if(first_char != 't' && first_char != 'f'){
        #ifdef JSMN_HELPER_LOG_ERRORS
        internalwarningprintf("jsmn_helper_get_boolean() expected boolean, got a different primitive\n");
        printf("   Got: ");
        jsmn_helper_print_token(buffer, tokens[index]);
        #endif
        return false;
    }

    *out_value = first_char == 't';
    return true;
}

length_t jsmn_helper_subtoken_count(jsmntok_t *tokens, length_t start_index){
    length_t index = start_index;
    jsmntok_t token = tokens[index++];
    switch(token.type){
    case JSMN_UNDEFINED: case JSMN_PRIMITIVE: return 1;
    case JSMN_OBJECT: case JSMN_STRING: case JSMN_ARRAY:
        for(int i = 0; i != token.size; i++){
            index += jsmn_helper_subtoken_count(tokens, index);
        }
        return index - start_index;
    }
    return 0;
}

weak_cstr_t jsmn_helper_parse_fail_reason(int code){
    switch(code){
    case JSMN_ERROR_INVAL: return "JSON is corrupted";
    case JSMN_ERROR_NOMEM: return "out of memory";
    case JSMN_ERROR_PART:  return "JSON is incomplete";
    }
    return "UNKNOWN";
}
