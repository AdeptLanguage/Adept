
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "DRVR/config.h"
#include "UTIL/color.h"
#include "UTIL/download.h"
#include "UTIL/filename.h"
#include "UTIL/ground.h"
#include "UTIL/jsmn_helper.h"

static successful_t config_read_adept_config_value(config_t *config, jsmnh_obj_ctx_t *parent_ctx, jsmntok_t *out_maybe_last_update);

#ifdef ADEPT_ENABLE_PACKAGE_MANAGER
static successful_t config_update_last_updated(weak_cstr_t filename, jsmnh_buffer_t buffer, jsmntok_t last_update);
static successful_t update_installation(config_t *config, download_buffer_t dlbuffer);
static successful_t process_adept_stash_value(jsmnh_obj_ctx_t *parent_ctx, stash_header_t *out_header);
#endif

void config_prepare(config_t *config){
    memset(config, 0, sizeof(config_t));
}

void config_free(config_t *config){
    free(config->stash);
}

successful_t config_read(config_t *config, weak_cstr_t filename, weak_cstr_t *out_warning){
    char *raw_buffer = NULL;
    length_t raw_buffer_length;

    FILE *f = fopen(filename, "rb");
    if(f == NULL){
        *out_warning = "Adept failed to read local configuration file, adept.config";
        return false;
    }

    fseek(f, 0, SEEK_END);
    raw_buffer_length = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    raw_buffer = malloc(raw_buffer_length + 1);
    if(raw_buffer == NULL){
        fclose(f);
        *out_warning = "Adept failed to read local configuration file due to not enough memory";
        return false;
    }

    fread(raw_buffer, 1, raw_buffer_length, f);
    fclose(f);

    raw_buffer[raw_buffer_length] = 0x00;

    jsmnh_obj_ctx_t ctx;
    if(jsmnh_obj_ctx_easy_init(&ctx, raw_buffer, raw_buffer_length)){
        redprintf("Failed to parse %s\n", filename_name_const(filename));
        goto failure;
    }

    jsmntok_t maybe_last_update;

    for(length_t section = 0; section != ctx.total_sections; section++){
        if(jsmnh_obj_ctx_read_key(&ctx)){
            *out_warning = "Configuration file expected section key";
            goto failure;
        }

        if(jsmnh_obj_ctx_eq(&ctx, "adept.config")){
            // Config information
            if(!config_read_adept_config_value(config, &ctx, &maybe_last_update)){
                redprintf("Failed to handle value in configuration file\n");
                goto failure;
            }
        } else if(jsmnh_obj_ctx_eq(&ctx, "installs")){
            // Local imports
        } else {
            yellowprintf("Invalid section key '%s' in configuration file, %s\n", ctx.value.content, filename_name_const(filename));
        }

        jsmnh_obj_ctx_blind_advance(&ctx);
    }

    config->has = true;

    #ifdef ADEPT_ENABLE_PACKAGE_MANAGER
    bool should_update = false;
    
    switch(config->update){
    case UPDATE_SCHEDULE_NEVER:
        break;
    case UPDATE_SCHEDULE_EVERYTIME:
        should_update = true;
        break;
    case UPDATE_SCHEDULE_HOURLY:
        should_update = config->last_updated + 3600 <= time(NULL);
        break;
    case UPDATE_SCHEDULE_DAILY:
        should_update = config->last_updated + 86400 <= time(NULL);
        break;
    case UPDATE_SCHEDULE_WEEKLY:
        should_update = config->last_updated + 604800 <= time(NULL);
        break;
    case UPDATE_SCHEDULE_MONTHLY:
        should_update = config->last_updated + 2592000 <= time(NULL);
        break;
    case UPDATE_SCHEDULE_YEARLY:
        should_update = config->last_updated + 31536000 <= time(NULL);
        break;
    }

    if(should_update){
        if(config->show_checking_for_updates_message){
            blueprintf("NOTE: Checking for updates as scheduled in 'adept.config'\n");
        }

        // Ignore failure to update last updated
        if(maybe_last_update.type == JSMN_PRIMITIVE)
            config_update_last_updated(filename, ctx.fulltext, maybe_last_update);

        download_buffer_t dlbuffer;
        if(download_to_memory(config->stash, &dlbuffer)){
            update_installation(config, dlbuffer);
            free(dlbuffer.bytes);
        } else {
            blueprintf("NOTE: Failed to check for updates as scheduled in 'adept.config', internet address unreachable\n");
        }
    }
    #endif // ADEPT_ENABLE_PACKAGE_MANAGER

    jsmnh_obj_ctx_free(&ctx);
    free(raw_buffer);
    return true;

failure:
    jsmnh_obj_ctx_free(&ctx);
    free(raw_buffer);
    return false;
}

static successful_t config_read_adept_config_value(config_t *config, jsmnh_obj_ctx_t *parent_ctx, jsmntok_t *out_maybe_last_update){
    jsmnh_obj_ctx_t fork;
    if(jsmnh_obj_ctx_subobject(parent_ctx, &fork)) return false;

    for(length_t i = 0; i != fork.total_sections; i++){
        if(jsmnh_obj_ctx_read_key(&fork)) goto failure;

        if(jsmnh_obj_ctx_eq(&fork, "lastUpdated")){
            if(!jsmnh_obj_ctx_get_integer(&fork, &config->last_updated)){
                warningprintf("Failed to parse integer value for label '%s'\n", fork.value.content);
                goto failure;
            }

            *out_maybe_last_update = fork.tokens.tokens[fork.token_index];
        } else if(jsmnh_obj_ctx_eq(&fork, "update")){
            if(!jsmnh_obj_ctx_get_fixed_string(&fork, fork.value.content, fork.value.capacity)){
                warningprintf("Failed to parse string value for label '%s'\n", fork.value.content);
                goto failure;
            }
            
            if     (jsmnh_obj_ctx_eq(&fork, "never"))     config->update = UPDATE_SCHEDULE_NEVER;
            else if(jsmnh_obj_ctx_eq(&fork, "always"))    config->update = UPDATE_SCHEDULE_EVERYTIME; // aka everytime
            else if(jsmnh_obj_ctx_eq(&fork, "everytime")) config->update = UPDATE_SCHEDULE_EVERYTIME;
            else if(jsmnh_obj_ctx_eq(&fork, "hourly"))    config->update = UPDATE_SCHEDULE_HOURLY;
            else if(jsmnh_obj_ctx_eq(&fork, "daily"))     config->update = UPDATE_SCHEDULE_DAILY;
            else if(jsmnh_obj_ctx_eq(&fork, "weekly"))    config->update = UPDATE_SCHEDULE_WEEKLY;
            else if(jsmnh_obj_ctx_eq(&fork, "monthly"))   config->update = UPDATE_SCHEDULE_MONTHLY;
            else if(jsmnh_obj_ctx_eq(&fork, "yearly"))    config->update = UPDATE_SCHEDULE_YEARLY;
            else                                          config->update = UPDATE_SCHEDULE_NEVER;
        } else if(jsmnh_obj_ctx_eq(&fork, "stash")){
            strong_cstr_t new_stash_location;

            if(!jsmnh_obj_ctx_get_variable_string(&fork, &new_stash_location)){
                warningprintf("Failed to parse string value for label '%s'\n", fork.value.content);
                goto failure;
            }

            free(config->stash);
            config->stash = new_stash_location;
        } else if(jsmnh_obj_ctx_eq(&fork, "showNewCompilerAvailable")){
            if(!jsmnh_obj_ctx_get_boolean(&fork, &config->show_new_compiler_available)){
                warningprintf("Failed to parse boolean value for label '%s'\n", fork.value.content);
                goto failure;
            }
        } else if(jsmnh_obj_ctx_eq(&fork, "showCheckingForUpdatesMessage")){
            if(!jsmnh_obj_ctx_get_boolean(&fork, &config->show_checking_for_updates_message)){
                warningprintf("Failed to parse boolean value for label '%s'\n", fork.value.content);
                goto failure;
            }
        } else {
            warningprintf("Ignoring unrecognized option '%s' in adept configuration file\n", fork.value.content);
        }

        jsmnh_obj_ctx_blind_advance(&fork);
    }
    
    jsmnh_obj_ctx_free(&fork);
    return true;

failure:
    jsmnh_obj_ctx_free(&fork);
    return false;
}

#ifdef ADEPT_ENABLE_PACKAGE_MANAGER
    static successful_t config_update_last_updated(weak_cstr_t filename, jsmnh_buffer_t buffer, jsmntok_t last_update){
        if(last_update.type != JSMN_PRIMITIVE) return false;
    
        FILE *f = fopen(filename, "wb");
        if(f == NULL) return false;
    
        fwrite(buffer.content, 1, last_update.start, f);
    
        // Since we are using binary file stream mode, don't rely on fprintf
        time_t timestamp = time(NULL);
        char timestamp_buffer[256];
        sprintf(timestamp_buffer, "%zu", (size_t) timestamp);
        fwrite(timestamp_buffer, 1, strlen(timestamp_buffer), f);
        
        fwrite(buffer.content + last_update.end, 1, buffer.capacity - last_update.end, f);
        fclose(f);
    
        return true;
    }
    
    static successful_t update_installation(config_t *config, download_buffer_t dlbuffer){
        jsmnh_obj_ctx_t ctx;
    
        if(jsmnh_obj_ctx_easy_init(&ctx, dlbuffer.bytes, dlbuffer.length)){
            redprintf("Failed to parse downloaded JSON\n");
            goto failure;
        }
    
        jsmntok_t maybe_last_update;
        memset(&maybe_last_update, 0, sizeof(jsmntok_t));
    
        for(length_t section = 0; section != ctx.total_sections; section++){
            if(jsmnh_obj_ctx_read_key(&ctx)){
                redprintf("Failed to process downloaded JSON\n");
                goto failure;
            }
    
            if(jsmnh_obj_ctx_eq(&ctx, "adept.stash")){
                // Stash Header
                stash_header_t stash_header;
                memset(&stash_header, 0, sizeof(stash_header_t));
    
                if(!process_adept_stash_value(&ctx, &stash_header)){
                    redprintf("Failed to handle value while processing stash header of downloaded JSON file\n");
                    goto failure;
                }
    
                if(
                    !streq(stash_header.latest_compiler_version, ADEPT_VERSION_STRING) &&
                    !streq(stash_header.latest_compiler_version, ADEPT_PREVIOUS_STABLE_VERSION_STRING) &&
                    config->show_new_compiler_available
                ){
                    blueprintf("\nNEWS: A newer version of Adept is available!\n");
                    printf("    Visit https://github.com/AdeptLanguage/Adept for more information!\n");
                    printf("    You can disable update checks in your 'adept.config'\n\n");
                    goto success;
                } else if(config->show_checking_for_updates_message){
                    blueprintf(" -> Already up to date!\n");
                }
    
                // Free potentially allocated values
                free(stash_header.latest_compiler_version);
            } else {
                // Ignore unrecognized keys
            }
    
            jsmnh_obj_ctx_blind_advance(&ctx);
        }
    
    success:
        jsmnh_obj_ctx_free(&ctx);
        return true;
    
    failure:
        jsmnh_obj_ctx_free(&ctx);
        return false;
    }

    static successful_t process_adept_stash_value(jsmnh_obj_ctx_t *parent_ctx, stash_header_t *out_header){
        jsmnh_obj_ctx_t fork;
        if(jsmnh_obj_ctx_subobject(parent_ctx, &fork)) goto failure;
    
        for(length_t i = 0; i != fork.total_sections; i++){
            if(jsmnh_obj_ctx_read_key(&fork)) goto failure;
    
            if(jsmnh_obj_ctx_eq(&fork, "minSpecVersion")){
                if(!jsmnh_obj_ctx_get_integer(&fork, &out_header->min_spec_version)) goto failure;
            } else if(jsmnh_obj_ctx_eq(&fork, "lastUpdated")){
                if(!jsmnh_obj_ctx_get_integer(&fork, &out_header->last_updated)) goto failure;
            } else if(jsmnh_obj_ctx_eq(&fork, "latestCompilerVersion")){
                if(!jsmnh_obj_ctx_get_variable_string(&fork, &out_header->latest_compiler_version)) goto failure;
            } else {
                // Ignore unknown keys
            }
    
            jsmnh_obj_ctx_blind_advance(&fork);
        }
    
        jsmnh_obj_ctx_free(&fork);
        return true;
    
    failure:
        jsmnh_obj_ctx_free(&fork);
        return false;
    }
#endif
