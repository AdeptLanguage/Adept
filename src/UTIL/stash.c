
#ifdef ADEPT_ENABLE_PACKAGE_MANAGER

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>

    #define makedir(a) mkdir(a)
#else
    #include <sys/stat.h>

    #define makedir(a) mkdir(a, 0777)
#endif

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DRVR/config.h"
#include "UTIL/color.h"
#include "UTIL/download.h"
#include "UTIL/filename.h"
#include "UTIL/ground.h"
#include "UTIL/jsmn_helper.h"
#include "UTIL/stash.h"
#include "UTIL/string.h"

successful_t adept_install(config_t *config, weak_cstr_t root, weak_cstr_t raw_identifier){
    strong_cstr_t identifier = strclone(raw_identifier);

    // Make identifier lowercase
    for(char *p = identifier; *p; p++) *p = tolower(*p);

    blueprintf(" - Locating       ");
    printf("%s\n", identifier);

    download_buffer_t dlbuffer;
    if(download_to_memory(config->stash, &dlbuffer) == false){   
        redprintf("Internet address for stash could not be reached\n");
        goto failure;
    }

    maybe_null_strong_cstr_t location = adept_locate_package(identifier, dlbuffer.bytes, dlbuffer.length);

    if(location == NULL){
        redprintf("   Failed to find ");
        printf("%s\n", identifier);
        free(dlbuffer.bytes);
        goto failure;
    }

    free(dlbuffer.bytes);
    if(download_to_memory(location, &dlbuffer) == false){   
        redprintf("Internet address for package location could not be reached\n");
        free(dlbuffer.bytes);
        goto failure;
    }

    blueprintf(" - Copying        ");
    printf("%s\n", identifier);

    if(adept_install_stash(config, root, dlbuffer.bytes, dlbuffer.length) == false){
        redprintf("   Failed to install ")
        printf("%s\n", identifier);
        free(dlbuffer.bytes);
        goto failure;
    }

    blueprintf(" - Installed      ");
    printf("%s\n", identifier);

    free(dlbuffer.bytes);
    free(identifier);
    return true;

failure:
    free(identifier);
    return false;
}

maybe_null_strong_cstr_t adept_locate_package(weak_cstr_t identifier, char *buffer, length_t length){
    jsmn_parser parser;
    jsmn_init(&parser);

    length_t required_tokens = jsmn_parse(&parser, buffer, length, NULL, 0);
    jsmntok_t *tokens = malloc(sizeof(jsmntok_t) * required_tokens);

    jsmn_init(&parser);
    int parse_error = jsmn_parse(&parser, buffer, length, tokens, required_tokens);
    
    if(parse_error < 0 || (int) required_tokens != parse_error){
        goto failure;
    }

    if(!jsmn_helper_get_object(buffer, tokens, required_tokens, 0)){
        goto failure;
    }

    jsmntok_t master_object_token = tokens[0];

    char key[256];
    length_t token_index = 1;
    length_t num_tokens = required_tokens;
    length_t total_sections = master_object_token.size;

    // TODO: CLEANUP: Clean up this dirty code
    for(length_t section = 0; section != total_sections; section++){
        if(!jsmn_helper_get_string(buffer, tokens, num_tokens, token_index++, key, sizeof(key))){
            goto failure;
        }

        char content[1024];
        length_t max_content = 1024;

        if(strcmp(key, "adept.stash") == 0){
            // Metadata

            if(!jsmn_helper_get_object(buffer, tokens, num_tokens, token_index)){
                goto failure;
            }

            jsmntok_t children_object_token = tokens[token_index++];
            
            for(int i = 0; i != children_object_token.size; i++){
                if(!jsmn_helper_get_string(buffer, tokens, num_tokens, token_index++, content, max_content)){
                    goto failure;
                }

                if(strcmp(content, "minSpecVersion") == 0){
                    long long min_spec_version;
                    if(!jsmn_helper_get_integer(buffer, tokens, num_tokens, token_index, &min_spec_version)){
                        goto failure;
                    }

                    if (min_spec_version > ADEPT_PACKAGE_MANAGER_SPEC_VERSION){
                        redprintf("Remote stash has minimum spec version of '%d', but compiler has spec version '%d'\n", (int) min_spec_version, ADEPT_PACKAGE_MANAGER_SPEC_VERSION);
                        redprintf("   (The remote stash no longer supports this compiler version)\n");
                        goto silent_failure;
                    }
                }

                token_index += jsmn_helper_subtoken_count(tokens, token_index);
            }
        } else if(strcmp(key, "children") == 0){
            // Locations

            if(!jsmn_helper_get_object(buffer, tokens, num_tokens, token_index)){
                goto failure;
            }

            jsmntok_t children_object_token = tokens[token_index++];

            for(int i = 0; i != children_object_token.size; i++){
                if(!jsmn_helper_get_string(buffer, tokens, num_tokens, token_index++, content, max_content)){
                    goto failure;
                }

                if(strcmp(content, identifier) == 0){
                    if(!jsmn_helper_get_string(buffer, tokens, num_tokens, token_index, content, max_content)){
                        goto failure;
                    }

                    free(tokens);               
                    return strclone(content);
                }

                token_index += jsmn_helper_subtoken_count(tokens, token_index);
            }
        } else {
            // Ignore everything else
            token_index += jsmn_helper_subtoken_count(tokens, token_index);
        }
    }

    goto silent_failure;

failure:
    redprintf("Failed to parse master stash file\n");

silent_failure:
    free(tokens);
    return NULL;
}

static successful_t perform_procedure_command(char *buffer, jsmntok_t *tokens, length_t num_tokens, length_t token_index, weak_cstr_t root, maybe_null_weak_cstr_t host){
    if(!jsmn_helper_get_object(buffer, tokens, num_tokens, token_index)) return false;

    jsmntok_t command_object_token = tokens[token_index++];
    length_t total_sections = command_object_token.size;
    
    char key[64];
    maybe_null_strong_cstr_t file = NULL;
    maybe_null_strong_cstr_t folder = NULL;
    maybe_null_strong_cstr_t from = NULL;

    for(length_t section = 0; section != total_sections; section++){
        if(!jsmn_helper_get_string(buffer, tokens, num_tokens, token_index++, key, sizeof(key))){
            goto failure;
        }

        if(strcmp(key, "file") == 0){
            if(!jsmn_helper_get_vstring(buffer, tokens, num_tokens, token_index++, &file)){
                goto failure;
            }
        } else if(strcmp(key, "folder") == 0){
            if(!jsmn_helper_get_vstring(buffer, tokens, num_tokens, token_index++, &folder)){
                goto failure;
            }
        } else if(strcmp(key, "from") == 0){
            if(!jsmn_helper_get_vstring(buffer, tokens, num_tokens, token_index++, &from)){
                goto failure;
            }
        } else {
            token_index += jsmn_helper_subtoken_count(tokens, token_index);
        }
    }

    if(folder){
        makedir(folder);
        goto success;
    }

    if(file && from){
        strong_cstr_t to = filename_local(root, file);
        strong_cstr_t url = host ? filename_local(host, from) : strclone(from);

        if(!download(url, to)){
            redprintf("Failed to download %s\n", url);
            free(to);
            free(url);
            goto failure;
        }

        free(to);
        free(url);
    }

success:
    free(file);
    free(folder);
    free(from);
    return true;

failure:
    free(file);
    free(folder);
    free(from);
    return false;
}

successful_t adept_install_stash(config_t *config, weak_cstr_t root, char *buffer, length_t length){
    (void) config; // unused

    jsmn_parser parser;
    jsmn_init(&parser);

    length_t required_tokens = jsmn_parse(&parser, buffer, length, NULL, 0);
    jsmntok_t *tokens = malloc(sizeof(jsmntok_t) * required_tokens);

    jsmn_init(&parser);
    int parse_error = jsmn_parse(&parser, buffer, length, tokens, required_tokens);
    
    if(parse_error < 0 || (int) required_tokens != parse_error){
        goto failure;
    }

    if(!jsmn_helper_get_object(buffer, tokens, required_tokens, 0)){
        goto failure;
    }

    jsmntok_t master_object_token = tokens[0];
    maybe_null_strong_cstr_t host = NULL;

    char key[256];
    length_t token_index = 1;
    length_t num_tokens = required_tokens;
    length_t total_sections = master_object_token.size;

    for(length_t section = 0; section != total_sections; section++){
        if(!jsmn_helper_get_string(buffer, tokens, num_tokens, token_index++, key, sizeof(key))){
            printf("1\n");
            goto failure;
        }

        if(strcmp(key, "host") == 0){
            if(!jsmn_helper_get_vstring(buffer, tokens, required_tokens, token_index++, &host)){
                goto failure;
            }
        } else if(strcmp(key, "procedure") == 0){
            // Locations

            if(!jsmn_helper_get_array(buffer, tokens, num_tokens, token_index)){
                goto failure;
            }

            jsmntok_t procedure_object_token = tokens[token_index++];

            for(int i = 0; i != procedure_object_token.size; i++){
                if(!jsmn_helper_get_object(buffer, tokens, required_tokens, token_index)){
                    goto failure;
                }

                if(!perform_procedure_command(buffer, tokens, num_tokens, token_index, root, host)){
                    goto failure;
                }

                token_index += jsmn_helper_subtoken_count(tokens, token_index);
            }
        } else {
            // Ignore everything else
            token_index += jsmn_helper_subtoken_count(tokens, token_index);
        }
    }

    return true;

failure:
    return false;
}

successful_t adept_uninstall(config_t *config, weak_cstr_t identifier){
    (void) config;
    (void) identifier;

    return false;
}

successful_t adept_info(config_t *config, weak_cstr_t identifier){
    (void) config;
    (void) identifier;
    
    return false;
}

#endif
