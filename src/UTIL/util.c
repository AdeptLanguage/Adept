
#if __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <stdarg.h>
#include "UTIL/util.h"
#include "UTIL/color.h"

void expand(void **inout_memory, length_t unit_size, length_t length, length_t *inout_capacity, length_t amount, length_t default_capacity){
    // Expands an array in memory to be able to fit more units

    if(*inout_capacity == 0 && amount != 0){
        *inout_memory = malloc(unit_size * default_capacity);
        *inout_capacity = default_capacity;
    }

    while(length + amount > *inout_capacity){
        *inout_capacity *= 2;
        
        #ifdef UTIL_USE_REALLOC
        *inout_memory = realloc(*inout_memory, unit_size * *inout_capacity);
        #else
        void *new_memory = malloc(unit_size * *inout_capacity);
        memcpy(new_memory, *inout_memory, unit_size * length);
        free(*inout_memory);
        *inout_memory = new_memory;
        #endif
    }
}

void coexpand(void **inout_memory1, length_t unit_size1, void **inout_memory2, length_t unit_size2, length_t length, length_t *inout_capacity, length_t amount, length_t default_capacity){
    // Expands an array in memory to be able to fit more units

    if(*inout_capacity == 0 && amount != 0){
        *inout_memory1 = malloc(unit_size1 * default_capacity);
        *inout_memory2 = malloc(unit_size2 * default_capacity);
        *inout_capacity = default_capacity;
    }

    while(length + amount > *inout_capacity){
        *inout_capacity *= 2;

        #ifdef UTIL_USE_REALLOC
        *inout_memory1 = realloc(*inout_memory1, unit_size1 * *inout_capacity);
        *inout_memory2 = realloc(*inout_memory2, unit_size2 * *inout_capacity);
        #else
        void *new_memory = malloc(unit_size1 * *inout_capacity);
        memcpy(new_memory, *inout_memory1, unit_size1 * length);
        free(*inout_memory1);
        *inout_memory1 = new_memory;

        new_memory = malloc(unit_size2 * *inout_capacity);
        memcpy(new_memory, *inout_memory2, unit_size2 * length);
        free(*inout_memory2);
        *inout_memory2 = new_memory;
        #endif
    }
}

#ifdef UTIL_USE_REALLOC
void grow_impl(void **inout_memory, length_t unit_size, length_t new_length){
#else
void grow_impl(void **inout_memory, length_t unit_size, length_t old_length, length_t new_length){
#endif
    #ifdef UTIL_USE_REALLOC
    *inout_memory = realloc(*inout_memory, unit_size * new_length);
    #else
    void *memory = malloc(unit_size * new_length);
    memcpy(memory, *inout_memory, unit_size * old_length);
    free(*inout_memory);
    *inout_memory = memory;
    #endif
}

strong_cstr_t strclone(const char *src){
    length_t size = strlen(src) + 1;
    char *clone = malloc(size);
    memcpy(clone, src, size);
    return clone;
}

void freestrs(strong_cstr_t *array, length_t length){
    for(length_t i = 0; i != length; i++) free(array[i]);
    free(array);
}

char *mallocandsprintf(const char *format, ...){
    // TODO: Add support for things other then '%s' and '%%'
    char *destination = NULL;
    va_list args;
    va_list transverse;
    va_start(args, format);
    va_copy(transverse, args);
    const char *p = format;
    size_t size = 1;
    while(*p != '\0'){
        if(*(p++) == '%'){
            switch(*(p++)){
            case '%':
                size++;
                break;
            case 's':
                size += strlen(va_arg(transverse, char*));
                break;
            default:
                internalwarningprintf("mallocandsprintf() received non %%s in format\n");
                size += 50; // Assume whatever it is, it will fit in 50 bytes
            }
        } else size++;
    }
    va_end(transverse);
    destination = malloc(size);
    vsprintf(destination, format, args);
    va_end(args);
    return destination;
}

strong_cstr_t string_to_escaped_string(char *array, length_t length, char escaped_quote){
    length_t put_index = 1;
    length_t special_characters = 0;

    // Count number of special characters (\n, \t, \b, etc.)
    for(length_t i = 0; i != length; i++){
        if(array[i] <= 0x1F || array[i] == '\\' || array[i] == escaped_quote) special_characters++;
    }

    strong_cstr_t string = malloc(length + special_characters + 3);
    string[0] = escaped_quote;
    
    for(length_t i = 0; i != length; i++){
        if(array[i] <= 0x1F || array[i] == '\\' || array[i] == escaped_quote){
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
        case '\e': string[put_index++] =  'e'; break;
        case '\\': string[put_index++] = '\\'; break;
        default:
            if(array[i] == escaped_quote){
                string[put_index++] = escaped_quote;
                break;
            }

            // Unrecognized special character, don't escape
            string[put_index - 1] = array[i];
        }
    }

    string[put_index++] = escaped_quote;
    string[put_index++] = '\0';
    return string;
}

length_t string_count_character(weak_cstr_t string, length_t length, char character){
    length_t count = 0;
    for(length_t i = 0; i != length; i++) if(string[i] == character) count++;
    return count;
}

length_t find_insert_position(void *array, length_t length, int(*compare)(const void*, const void*), void *object_reference, length_t object_size){
    maybe_index_t first, middle, last, comparison;
    first = 0; last = length - 1;

    while(first <= last){
        middle = (first + last) / 2;
        comparison = compare((char*) array + (middle * object_size), object_reference);
        
        if(comparison == 0) return middle;
        else if(comparison > 0) last = middle - 1;
        else first = middle + 1;
    }

    return first;
}

#if __EMSCRIPTEN__
EM_JS(int, node_fs_existsSync, (const char *filename), {
    var fs = require('fs');
    return fs.existsSync(UTF8ToString(filename)) ? 1 : 0;
});

    EM_JS(strong_cstr_t, node_fs_readFileSync, (const char *filename, bool will_append_newline), {
    var fs = require('fs');

    try {
        contents = fs.readFileSync(UTF8ToString(filename), "utf8");
    } catch(error){
        return null;
    }

    bytes = lengthBytesUTF8(contents);
    ptr = _malloc(bytes + (will_append_newline ? 2 : 1));
    stringToUTF8(contents, ptr, bytes + 1);
    return ptr;
});

EM_JS(strong_cstr_t, node_fs_readFileSync_binary_hex, (const char *filename), {
    var fs = require('fs');

    try {
        contents = fs.readFileSync(UTF8ToString(filename)).toString('hex');
    } catch(error){
        return null;
    }

    bytes = lengthBytesUTF8(contents);
    ptr = _malloc(bytes + 1);
    stringToUTF8(contents, ptr, bytes + 1);
    return ptr;
});
#endif

bool file_exists(weak_cstr_t filename){
    #if __EMSCRIPTEN__
    return node_fs_existsSync(filename) != 0;
    #else
    FILE *f = fopen(filename, "r");
    if(f != NULL){
        fclose(f);
        return true;
    }
    return false;
    #endif
}

bool file_text_contents(weak_cstr_t filename, strong_cstr_t *out_contents, length_t *out_length, bool append_newline){
    char *buffer;
    length_t buffer_size;

    #if __EMSCRIPTEN__
    buffer = node_fs_readFileSync(filename, append_newline);
    if(buffer == NULL) return false;

    buffer_size = strlen(buffer);
    #else
    FILE *file = fopen(filename, "r");
    if(file == NULL) return false;

    fseek(file, 0, SEEK_END);
    buffer_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    buffer = malloc(buffer_size + 2);
    buffer_size = fread(buffer, 1, buffer_size, file);
    fclose(file);
    #endif

    if(append_newline){
        buffer[buffer_size] = '\n';   // Append newline to flush everything
        buffer[++buffer_size] = '\0'; // Terminate the string for good measure
    }

    *out_contents = buffer;
    *out_length = buffer_size;
    return true;
}

bool file_binary_contents(weak_cstr_t filename, strong_cstr_t *out_contents, length_t *out_length){
    char *buffer;
    length_t buffer_size;

    #if __EMSCRIPTEN__
    char *hex = node_fs_readFileSync_binary_hex(filename);
    if(hex == NULL) return false;

    buffer_size = strlen(hex) / 2;
    buffer = malloc(buffer_size);

    // NOTE: Assumes proper hexadecimal
    for(length_t i = 0; i != buffer_size; i++){
        char bigger = hex[i * 2];
        char lesser = hex[i * 2 + 1];

        char bigger_value = bigger < 'A'? bigger - '0' : bigger < 'a' ? bigger - 'A' + 10 : bigger - 'a' + 10;
        char lesser_value = lesser < 'A'? lesser - '0' : lesser < 'a' ? lesser - 'A' + 10 : lesser - 'a' + 10;
        buffer[i] = bigger_value * 16 + lesser_value;
    }

    free(hex);
    #else
    FILE *file = fopen(filename, "rb");
    if(file == NULL) return false;

    fseek(file, 0, SEEK_END);
    buffer_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    buffer = malloc(buffer_size + 1);
    buffer_size = fread(buffer, 1, buffer_size, file);
    fclose(file);
    #endif

    *out_contents = buffer;
    *out_length = buffer_size;
    return true;
}
