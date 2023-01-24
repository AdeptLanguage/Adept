
#include "LEX/pkg.h"

#include <stdlib.h>
#include <string.h>

#include "TOKEN/token_data.h"
#include "UTIL/string.h"
#include "UTIL/color.h"
#include "UTIL/datatypes.h"
#include "UTIL/search.h"

errorcode_t pkg_write(const char *filename, tokenlist_t *tokenlist){
    FILE *file = fopen(filename, "wb");
    if(file == NULL) return FAILURE;

    /*
        FILE STRUCTURE:
        pkg_version_t - sizeof(pkg_version_t)
        pkg_header_t  - sizeof(pkg_header_t)
        <tokens>      - sizeof(char) * tokens + <extra data>

        TOKEN STRUCTURE:
        char id
        null terminated string if id has extra data
    }
    */

    token_t *tokens = tokenlist->tokens;
    length_t length = tokenlist->length;

    pkg_version_t pkg_version;
    pkg_version.magic_number = 0x74706461;
    pkg_version.endianness = 0x00EF;
    pkg_version.iteration_version = TOKEN_ITERATION_VERSION;

    pkg_header_t pkg_header;
    pkg_header.length = tokenlist->length;

    fwrite(&pkg_version, sizeof(pkg_version_t), 1, file);
    fwrite(&pkg_header, sizeof(pkg_header_t), 1, file);

    char buffer[1024];

    for(length_t t = 0; t != length; t++){
        tokenid_t id = tokens[t].id;

        if(tokens[t].id == TOKEN_WORD){
            if(pkg_compress_word(file, &tokens[t])){
                fclose(file);
                return FAILURE;
            }
        }
        else if(tokens[t].id == TOKEN_STRING || tokens[t].id == TOKEN_CSTRING){
            token_string_data_t *string_data = (token_string_data_t*) tokens[t].data;

            fwrite(&id, sizeof(tokenid_t), 1, file);
            fwrite(&string_data->length, sizeof(length_t), 1, file);
            fwrite(string_data->array, string_data->length, 1, file);
        }
        else if(tokens[t].id == TOKEN_GENERIC_INT){
            fwrite(&id, sizeof(tokenid_t), 1, file);
            snprintf(buffer, sizeof buffer, "%ld", (long) *((long long*) tokens[t].data));
            fwrite(buffer, strlen(buffer) + 1, 1, file);
        }
        else if(tokens[t].id == TOKEN_GENERIC_FLOAT){
            fwrite(&id, sizeof(tokenid_t), 1, file);
            snprintf(buffer, sizeof buffer, "%06.6f", *((double*) tokens[t].data));
            fwrite(buffer, strlen(buffer) + 1, 1, file);
        }
        else {
            fwrite(&id, sizeof(tokenid_t), 1, file);
        }
    }

    fclose(file);
    return SUCCESS;
}

errorcode_t pkg_read(object_t *object){
    #if __EMSCRIPTEN__
    // Don't support .dep files when running via emscripten
    redprintf("Adept doesn't support '.dep' files when running via emscripten\n");
    return FAILURE;
    #endif

    object->buffer = NULL;
    
    tokenlist_t *tokenlist = &object->tokenlist;
    FILE *file = fopen(object->filename, "rb");

    if(file == NULL){
        redprintf("The file '%s' doesn't exist or can't be accessed\n", object->filename);
        return FAILURE;
    }

    pkg_version_t pkg_version;
    pkg_header_t pkg_header;

    fread(&pkg_version, sizeof(pkg_version_t), 1, file);
    fread(&pkg_header, sizeof(pkg_header_t), 1, file);

    if(pkg_version.magic_number != 0x74706461){
        fprintf(stderr, "INTERNAL ERROR: Tried to read a package file '%s' that isn't a package\n", object->filename);
        fclose(file);
        return FAILURE;
    }

    if(pkg_version.endianness != 0x00EF){
        fprintf(stderr, "INTERNAL ERROR: Failed to read package '%s' because of mismatched endianness\n", object->filename);
        fclose(file);
        return FAILURE;
    }

    if(pkg_version.iteration_version != TOKEN_ITERATION_VERSION){
        fprintf(stderr, "INTERNAL ERROR: Incompatible package iteration version for package '%s'\n", object->filename);
        fclose(file);
        return FAILURE;
    }

    tokenlist->tokens = malloc(sizeof(token_t) * pkg_header.length);
    tokenlist->length = pkg_header.length;
    // tokenlist->capacity; ignored
    tokenlist->sources = malloc(sizeof(source_t) * pkg_header.length);
    object->compilation_stage = COMPILATION_STAGE_TOKENLIST;

    source_t *sources = tokenlist->sources;
    length_t sources_length = pkg_header.length;
    length_t target_object_index = object->index;
    object->traits |= OBJECT_PACKAGE;

    // Fill in source information
    for(length_t s = 0; s != sources_length; s++){
        // sources[s].index is never used
        sources[s].object_index = target_object_index;
        // sources[s].stride is never used
    }

    char buildup[1024];
    length_t buildup_length;

    for(length_t t = 0; t != pkg_header.length; t++){
        tokenid_t id;
        fread(&id, sizeof(tokenid_t), 1, file);
        tokenlist->tokens[t].id = id;

        if(id == TOKEN_WORD){
            char read;
            fread(&read, sizeof(char), 1, file);
            for(buildup_length = 0; read != '\0'; buildup_length++){
                if(buildup_length == 1024) {
                    fprintf(stderr, "Token extra data in '%s' exceeded 1024 which is currently unsupported\n", object->filename);
                    fclose(file);

                    // Override tokenlist length for safe recovery deletion
                    tokenlist->length = t;
                    return FAILURE;
                }
                buildup[buildup_length] = read;
                fread(&read, sizeof(char), 1, file);
            }
            tokenlist->tokens[t].data = memcpy(malloc(buildup_length + 1), buildup, buildup_length);
            ((char*)tokenlist->tokens[t].data)[buildup_length] = '\0';
        }
        else if(id == TOKEN_STRING || id == TOKEN_CSTRING){
            fread(&buildup_length, sizeof(length_t), 1, file);

            char *array = malloc(buildup_length + 1);
            fread(array, sizeof(char), buildup_length, file);
            array[buildup_length] = '\0'; // For lazy conversion to c-string

            tokenlist->tokens[t].data = malloc_init(token_string_data_t, {
                .array = array,
                .length = buildup_length,
            });;
        }
        else if(id == TOKEN_GENERIC_INT){
            char read;
            fread(&read, sizeof(char), 1, file);
            for(buildup_length = 0; read != '\0'; buildup_length++){
                if(buildup_length + 1 == 1024) {
                    fprintf(stderr, "Token extra data in '%s' exceeded 1024 which is currently unsupported\n", object->filename);
                    fclose(file);

                    // Override tokenlist length for safe recovery deletion
                    tokenlist->length = t;
                    return FAILURE;
                }
                buildup[buildup_length] = read;
                fread(&read, sizeof(char), 1, file);
            }
            buildup[buildup_length] = '\0';
            tokenlist->tokens[t].data = malloc(sizeof(adept_generic_int));
            *((adept_generic_int*) tokenlist->tokens[t].data) = string_to_int64(buildup, 10);
        }
        else if(id == TOKEN_GENERIC_FLOAT){
            char read;
            fread(&read, sizeof(char), 1, file);
            for(buildup_length = 0; read != '\0'; buildup_length++){
                if(buildup_length + 1 == 1024) {
                    fprintf(stderr, "Token extra data in '%s' exceeded 1024 which is currently unsupported\n", object->filename);
                    fclose(file);

                    // Override tokenlist length for safe recovery deletion
                    tokenlist->length = t;
                    return FAILURE;
                }
                buildup[buildup_length] = read;
                fread(&read, sizeof(char), 1, file);
            }
            buildup[buildup_length] = '\0';
            tokenlist->tokens[t].data = malloc(sizeof(double));
            *((double*) tokenlist->tokens[t].data) = atof(buildup);
        }
        else if(id >= TOKEN_PKG_MIN && id <= TOKEN_PKG_MAX){
            tokenlist->tokens[t].id = TOKEN_WORD;

            switch(id){
            case TOKEN_PKG_WBOOL:
                tokenlist->tokens[t].data = strclone("bool");
                break;
            case TOKEN_PKG_WBYTE:
                tokenlist->tokens[t].data = strclone("byte");
                break;
            case TOKEN_PKG_WUBYTE:
                tokenlist->tokens[t].data = strclone("ubyte");
                break;
            case TOKEN_PKG_WSHORT:
                tokenlist->tokens[t].data = strclone("short");
                break;
            case TOKEN_PKG_WUSHORT:
                tokenlist->tokens[t].data = strclone("ushort");
                break;
            case TOKEN_PKG_WINT:
                tokenlist->tokens[t].data = strclone("int");
                break;
            case TOKEN_PKG_WUINT:
                tokenlist->tokens[t].data = strclone("uint");
                break;
            case TOKEN_PKG_WLONG:
                tokenlist->tokens[t].data = strclone("long");
                break;
            case TOKEN_PKG_WULONG:
                tokenlist->tokens[t].data = strclone("ulong");
                break;
            case TOKEN_PKG_WFLOAT:
                tokenlist->tokens[t].data = strclone("float");
                break;
            case TOKEN_PKG_WDOUBLE:
                tokenlist->tokens[t].data = strclone("double");
                break;
            case TOKEN_PKG_WUSIZE:
                tokenlist->tokens[t].data = strclone("usize");
                break;
            }
        }
        else {
            tokenlist->tokens[t].data = NULL;
        }
    }

    fclose(file);
    return SUCCESS;
}

errorcode_t pkg_compress_word(FILE *file, token_t *token){
    const length_t compressible_words_length = 12;

    const char * const compressible_words[] = {
        "bool", "byte", "double", "float", "int", "long", "short", "ubyte", "uint", "ulong", "ushort", "usize"
    };

    maybe_index_t index = binary_string_search_const(compressible_words, compressible_words_length, token->data);

    if(index == -1){
        tokenid_t id = token->id;
        length_t word_length = strlen(token->data);
        fwrite(&id, sizeof(tokenid_t), 1, file);

        if(word_length > 1024){
            redprintf("Failed to create package because identifier exceeded max length of 1024 bytes!\n");
            fclose(file);
            return FAILURE;
        }

        fwrite(token->data, word_length + 1, 1, file);
    } else {
        tokenid_t shorthand_token = TOKEN_PKG_MIN + index;
        fwrite(&shorthand_token, sizeof(tokenid_t), 1, file);
    }

    return SUCCESS;
}
