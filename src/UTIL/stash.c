
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

    if(adept_install_stash(root, dlbuffer.bytes, dlbuffer.length) == false){
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

static errorcode_t adept_locate_package_handle_metadata(jsmnh_obj_ctx_t *parent_ctx){
    jsmnh_obj_ctx_t fork;
    if(jsmnh_obj_ctx_subobject(parent_ctx, &fork)) return FAILURE;

    for(length_t i = 0; i != fork.total_sections; i++){
        if(jsmnh_obj_ctx_read_key(&fork)){
            goto failure;
        }

        if(jsmnh_obj_ctx_eq(&fork, "minSpecVersion")){
            long long min_spec_version;
            if(!jsmnh_get_integer(fork.fulltext.content, fork.tokens, fork.token_index, &min_spec_version)){
                goto failure;
            }

            if(min_spec_version > ADEPT_PACKAGE_MANAGER_SPEC_VERSION){
                redprintf("Remote stash has minimum spec version of '%d', but compiler has spec version '%d'\n", (int) min_spec_version, ADEPT_PACKAGE_MANAGER_SPEC_VERSION);
                redprintf("   (The remote stash no longer supports this compiler version)\n");
                goto failure;
            }
        }

        jsmnh_obj_ctx_blind_advance(&fork);
    }

    return SUCCESS;

failure:
    jsmnh_obj_ctx_free(&fork);
    return FAILURE;
}

static errorcode_t adept_locate_package_handle_children(jsmnh_obj_ctx_t *parent_ctx, weak_cstr_t target_package_name, strong_cstr_t *output){
    jsmnh_obj_ctx_t fork;
    if(jsmnh_obj_ctx_subobject(parent_ctx, &fork)) return FAILURE;

    for(length_t i = 0; i != fork.total_sections; i++){
        if(jsmnh_obj_ctx_read_key(&fork)) goto failure;

        if(jsmnh_obj_ctx_eq(&fork, target_package_name)){
            if(!jsmnh_obj_ctx_get_variable_string(&fork, output)){
                goto failure;
            }

            jsmnh_obj_ctx_free(&fork);
            return SUCCESS;
        }

        jsmnh_obj_ctx_blind_advance(&fork);
    }

    *output = NULL;
    return SUCCESS;

failure:
    jsmnh_obj_ctx_free(&fork);
    return FAILURE;
}

maybe_null_strong_cstr_t adept_locate_package(weak_cstr_t package_name, char *raw_buffer, length_t raw_buffer_length){
    jsmnh_obj_ctx_t ctx;
    if(jsmnh_obj_ctx_easy_init(&ctx, raw_buffer, raw_buffer_length)) goto failure;

    // Handle values of JSON object
    for(length_t section = 0; section != ctx.total_sections; section++){
        if(jsmnh_obj_ctx_read_key(&ctx)) goto failure;

        if(jsmnh_obj_ctx_eq(&ctx, "adept.stash")){
            if(adept_locate_package_handle_metadata(&ctx)){
                goto failure;
            }
        } else if(jsmnh_obj_ctx_eq(&ctx, "children")){
            strong_cstr_t output = NULL;

            if(adept_locate_package_handle_children(&ctx, package_name, &output)){
                goto failure;
            }

            if(output){
                jsmnh_obj_ctx_free(&ctx);
                return output;
            }
        } else {
            // Ignore invalid keys
        }

        jsmnh_obj_ctx_blind_advance(&ctx);
    }

    goto silent_failure;

failure:
    redprintf("Failed to parse master stash file\n");

silent_failure:
    jsmnh_obj_ctx_free(&ctx);
    return NULL;
}

static successful_t perform_procedure_command(jsmnh_obj_ctx_t *parent_ctx, weak_cstr_t root, maybe_null_weak_cstr_t host){
    jsmnh_obj_ctx_t fork;
    if(jsmnh_obj_ctx_subobject(parent_ctx, &fork)) return false;

    maybe_null_strong_cstr_t file = NULL;
    maybe_null_strong_cstr_t folder = NULL;
    maybe_null_strong_cstr_t from = NULL;

    for(length_t section = 0; section != fork.total_sections; section++){
        if(jsmnh_obj_ctx_read_key(&fork)) goto failure;

        if(jsmnh_obj_ctx_eq(&fork, "file")){
            if(!jsmnh_obj_ctx_get_variable_string(&fork, &file)){
                goto failure;
            }
        } else if(jsmnh_obj_ctx_eq(&fork, "folder")){
            if(!jsmnh_obj_ctx_get_variable_string(&fork, &folder)){
                goto failure;
            }
        } else if(jsmnh_obj_ctx_eq(&fork, "from")){
            if(!jsmnh_obj_ctx_get_variable_string(&fork, &from)){
                goto failure;
            }
        } else {
            // Ignore unrecognized keys
        }

        jsmnh_obj_ctx_blind_advance(&fork);
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
    jsmnh_obj_ctx_free(&fork);
    free(file);
    free(folder);
    free(from);
    return true;

failure:
    jsmnh_obj_ctx_free(&fork);
    free(file);
    free(folder);
    free(from);
    return false;
}

successful_t adept_install_stash(weak_cstr_t root, char *raw_buffer, length_t raw_buffer_length){
    jsmnh_obj_ctx_t ctx;
    if(jsmnh_obj_ctx_easy_init(&ctx, raw_buffer, raw_buffer_length)) goto failure;

    maybe_null_strong_cstr_t host = NULL;

    for(length_t section = 0; section != ctx.total_sections; section++){
        if(jsmnh_obj_ctx_read_key(&ctx)) goto failure;

        if(jsmnh_obj_ctx_eq(&ctx, "host")){
            if(!jsmnh_obj_ctx_get_variable_string(&ctx, &host)){
                goto failure;
            }
            jsmnh_obj_ctx_blind_advance(&ctx);
        } else if(jsmnh_obj_ctx_eq(&ctx, "procedure")){
            // Locations

            if(!jsmnh_obj_ctx_get_array(&ctx)){
                goto failure;
            }

            jsmntok_t procedure_object_token = ctx.tokens.tokens[ctx.token_index++];

            for(length_t i = 0; i != (length_t) procedure_object_token.size; i++){
                if(!perform_procedure_command(&ctx, root, host)){
                    goto failure;
                }

                jsmnh_obj_ctx_blind_advance(&ctx);
            }
        } else {
            // Ignore unrecognized keys
            jsmnh_obj_ctx_blind_advance(&ctx);
        }
    }

    jsmnh_obj_ctx_free(&ctx);
    return true;

failure:
    jsmnh_obj_ctx_free(&ctx);
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
