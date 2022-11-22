
#include "UTIL/string.h"

#include <stdlib.h>
#include <string.h>

extern inline strong_cstr_t strclone(const char *src);
extern inline strong_cstr_t strong_cstr_empty_if_null(maybe_null_strong_cstr_t string);

strong_cstr_t *strsclone(strong_cstr_t *list, length_t length){
    strong_cstr_t *new_list = malloc(sizeof(strong_cstr_t) * length);

    for(length_t i = 0; i < length; i++){
        new_list[i] = strclone(list[i]);
    }

    return new_list;
}

void free_strings(strong_cstr_t *list, length_t length){
    for(length_t i = 0; i != length; i++) free(list[i]);
    free(list);
}

strong_cstr_t string_to_escaped_string(const char *array, length_t length, char escaped_quote, bool surround){
    length_t put_index = 0;
    length_t special_characters = 0;

    // Count number of special characters (\n, \t, \b, etc.)
    for(length_t i = 0; i != length; i++){
        if(array[i] <= 0x1F || array[i] == '\\' || array[i] == escaped_quote) special_characters++;
    }

    strong_cstr_t string = malloc(length + special_characters + 3);
    if(escaped_quote && surround) string[put_index++] = escaped_quote;
    
    for(length_t i = 0; i != length; i++){
        if(array[i] <= 0x1F || array[i] == '\\' || (array[i] == escaped_quote && escaped_quote)){
            // Escape special character
            string[put_index++] = '\\';
        } else {
            // Put regular character
            string[put_index++] = array[i];
            continue;
        }

        switch(array[i]){
        case '\0': string[put_index++] =  '0'; break;
        case '\t': string[put_index++] =  't'; break;
        case '\n': string[put_index++] =  'n'; break;
        case '\r': string[put_index++] =  'r'; break;
        case '\b': string[put_index++] =  'b'; break;
        case '\\': string[put_index++] = '\\'; break;
        case 0x1B: string[put_index++] =  'e'; break;
        default:
            if(array[i] == escaped_quote){
                string[put_index++] = escaped_quote;
                break;
            }

            // Unrecognized special character, don't escape
            string[put_index - 1] = array[i];
        }
    }

    if(escaped_quote && surround) string[put_index++] = escaped_quote;
    string[put_index++] = '\0';
    return string;
}


strong_cstr_t string_to_unescaped_string(const char *data, length_t length, length_t *out_length, string_unescape_error_t *out_error_cause){
    strong_cstr_t output = malloc(length + 1);
    length_t get = 0;
    length_t put = 0;

    while(get != length){
        if(data[get] != '\\'){
            output[put++] = data[get++];
            continue;
        }

        switch(data[get + 1]){
        case 'n': output[put++] = '\n';  break;
        case 'r': output[put++] = '\r';  break;
        case 't': output[put++] = '\t';  break;
        case 'b': output[put++] = '\b';  break;
        case '0': output[put++] = '\0';  break;
        case '"': output[put++] = '"';   break;
        case 'e': output[put++] = 0x1B;  break;
        case '\'': output[put++] = '\''; break;
        case '\\': output[put++] = '\\'; break;
        default:
            *out_error_cause = (string_unescape_error_t){
                .relative_position = get,
            };
            free(output);
            return NULL;
        }

        get += 2;
    }

    output[put] = '\0';
    *out_length = put;
    return output;
}

#ifdef ADEPT_INSIGHT_BUILD
// (insight only)
bool string_needs_escaping(weak_cstr_t string, char escaped_quote){
    // Look though string to see if there are any special characters (\n, \t, \b, etc.) that need escaping
    // ---
    // NOTE: Since the "no escaped quote" value of 'escaped_quote' is '\0', no additional logic is needed
    // Because a character of '\0' will never appear "inside of" a c-string.

    for(; *string; string++){
        if(*string <= 0x1F || *string == '\\' || *string == escaped_quote) return true;
    }

    return false;
}
#endif // ADEPT_INSIGHT_BUILD

length_t string_count_character(const char *string, length_t length, char character){
    length_t count = 0;
    for(length_t i = 0; i != length; i++) if(string[i] == character) count++;
    return count;
}

bool string_starts_with(weak_cstr_t original, weak_cstr_t stub){
    length_t original_length = strlen(original);
    length_t stub_length = strlen(stub);
    return stub_length <= original_length && strncmp(original, stub, stub_length) == 0;
}
