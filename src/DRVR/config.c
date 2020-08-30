
#include "UTIL/jsmn.h"
#include "UTIL/jsmn_helper.h"
#include "UTIL/util.h"
#include "UTIL/color.h"
#include "UTIL/filename.h"
#include "DRVR/config.h"
#include <time.h>
#include <inttypes.h>

#include "UTIL/download.h"

void config_prepare(config_t *config){
    memset(config, 0, sizeof(config_t));
}

void config_free(config_t *config){
    free(config->stash);
}

successful_t config_read(config_t *config, weak_cstr_t filename, weak_cstr_t *out_warning){
    strong_cstr_t buffer = NULL;
    length_t length;

    FILE *f = fopen(filename, "rb");
    if(f == NULL){
        *out_warning = "WARNING: Adept failed to read local configuration file, adept.config";
        return false;
    }

    fseek(f, 0, SEEK_END);
    length = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    buffer = malloc(length + 1);
    if(buffer == NULL){
        fclose(f);

        *out_warning = "WARNING: Adept failed to read local configuration file due to not enough memory";
        return false;
    }

    fread(buffer, 1, length, f);
    fclose(f);

    buffer[length] = 0x00;

    jsmn_parser parser;
    jsmn_init(&parser);

    length_t required_tokens = jsmn_parse(&parser, buffer, length, NULL, 0);
    jsmntok_t *tokens = malloc(sizeof(jsmntok_t) * required_tokens);

    jsmn_init(&parser);
    int parse_error = jsmn_parse(&parser, buffer, length, tokens, required_tokens);
    
    if(parse_error < 0 || required_tokens != parse_error){
        redprintf("Failed to parse %s, %s\n", filename_name_const(filename), jsmn_helper_parse_fail_reason(parse_error));
        free(tokens);
        free(buffer);
        return false;
    }

    if(!jsmn_helper_get_object(buffer, tokens, required_tokens, 0)){
        *out_warning = "WARNING: Invalid Adept Configuration File";
        free(tokens);
        free(buffer);
        return false;
    }

    jsmntok_t master_object_token = tokens[0];

    // Handle each section of configuration
    char key[256];
    length_t section_token_index = 1;
    length_t total_sections = master_object_token.size;

    jsmntok_t maybe_last_update;
    memset(&maybe_last_update, 0, sizeof(jsmntok_t));

    for(length_t section = 0; section != total_sections; section++){
        if(!jsmn_helper_get_string(buffer, tokens, required_tokens, section_token_index++, key, sizeof(key))){
            *out_warning = "WARNING: Configuration file expected section key";
            free(tokens);
            free(buffer);
            return false;
        }

        if(strcmp(key, "adept.config") == 0){
            // Config information
            if(!config_read_adept_config_value(config, buffer, tokens, required_tokens, section_token_index, &maybe_last_update)){
                redprintf("Failed to handle value in configuration file\n");
                return false;
            }
        } else if(strcmp(key, "imports") == 0){
            // Local imports
        } else {
            yellowprintf("WARNING: Invalid section key '%s' in configuration file, %s\n", key, filename_name_const(filename));
        }

        section_token_index += jsmn_helper_subtoken_count(tokens, section_token_index);
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
        blueprintf("NOTE: Automatically checking for updates as scheduled\n");

        // Ignore failure to update last updated
        if(maybe_last_update.type == JSMN_PRIMITIVE)
            config_update_last_updated(config, filename, buffer, length, maybe_last_update);

        download_buffer_t dlbuffer;
        if(download_to_memory(config->stash, &dlbuffer)){
            update_installation(config, dlbuffer);
            free(dlbuffer.bytes);
        } else {
            printf("(failed to download stash)\n");
        }
    }
    #endif // ADEPT_ENABLE_PACKAGE_MANAGER

    free(tokens);
    free(buffer);
    return true;
}

successful_t config_update_last_updated(config_t *config, weak_cstr_t filename, weak_cstr_t buffer, length_t buffer_length, jsmntok_t last_update){
    if(last_update.type != JSMN_PRIMITIVE) return false;

    FILE *f = fopen(filename, "wb");
    if(f == NULL) return false;

    fwrite(buffer, 1, last_update.start, f);

    // Since we are using binary file stream mode, don't rely on fprintf
    time_t timestamp = time(NULL);
    char timestamp_buffer[256];
    sprintf(timestamp_buffer, "%"PRIu64, (uint64_t) timestamp);
    fwrite(timestamp_buffer, 1, strlen(timestamp_buffer), f);
    
    fwrite(buffer + last_update.end, 1, buffer_length - last_update.end, f);
    fclose(f);

    return true;
}

successful_t config_read_adept_config_value(config_t *config, weak_cstr_t buffer, jsmntok_t *tokens, length_t num_tokens, length_t index, jsmntok_t *out_maybe_last_update){
    char content[1024];
    length_t max_content = 1024;
    long long integer_value;
    bool boolean;

    if(!jsmn_helper_get_object(buffer, tokens, num_tokens, index)){
        printf("WARNING: config_read_adept_config_value() -> jsmn_helper_get_object() failed\n");
        return false;
    }

    jsmntok_t main_object_token = tokens[index++];

    for(length_t i = 0; i != main_object_token.size; i++){
        if(!jsmn_helper_get_string(buffer, tokens, num_tokens, index, content, max_content)){
            printf("WARNING: config_read_adept_config_value() -> jsmn_helper_get_string() failed\n");
            return false;
        }

        if(strcmp(content, "lastUpdated") == 0){
            if(!jsmn_helper_get_integer(buffer, tokens, num_tokens, ++index, &integer_value)){
                printf("WARNING: Failed to parse integer value for label '%s'\n", content);
                return false;
            }
            config->last_updated = integer_value;
            *out_maybe_last_update = tokens[index];
            index++;
        } else if(strcmp(content, "update") == 0){
            if(!jsmn_helper_get_string(buffer, tokens, num_tokens, ++index, content, max_content)){
                printf("WARNING: Failed to parse string value for label '%s'\n", content);
                return false;
            }
            
            if     (strcmp(content, "never") == 0)     config->update = UPDATE_SCHEDULE_NEVER;
            else if(strcmp(content, "always") == 0)    config->update = UPDATE_SCHEDULE_EVERYTIME; // aka everytime
            else if(strcmp(content, "everytime") == 0) config->update = UPDATE_SCHEDULE_EVERYTIME;
            else if(strcmp(content, "hourly") == 0)    config->update = UPDATE_SCHEDULE_HOURLY;
            else if(strcmp(content, "daily") == 0)     config->update = UPDATE_SCHEDULE_DAILY;
            else if(strcmp(content, "weekly") == 0)    config->update = UPDATE_SCHEDULE_WEEKLY;
            else if(strcmp(content, "monthly") == 0)   config->update = UPDATE_SCHEDULE_MONTHLY;
            else if(strcmp(content, "yearly") == 0)    config->update = UPDATE_SCHEDULE_YEARLY;
            else                                       config->update = UPDATE_SCHEDULE_NEVER;
            index++;
        } else if(strcmp(content, "stash") == 0){
            if(!jsmn_helper_get_string(buffer, tokens, num_tokens, ++index, content, max_content)){
                printf("WARNING: Failed to parse string value for label '%s'\n", content);
                return false;
            }

            free(config->stash);
            config->stash = strclone(content);
            index++;
        } else if(strcmp(content, "showNewCompilerAvailable") == 0){
            if(!jsmn_helper_get_boolean(buffer, tokens, num_tokens, ++index, &boolean)){
                printf("WARNING: Failed to parse boolean value for label '%s'\n", content);
                return false;
            }

            config->show_new_compiler_available = boolean;
            index++;
        } else {
            yellowprintf("WARNING: Ignoring unrecognized option '%s' in adept configuration file\n", content);
            index += jsmn_helper_subtoken_count(tokens, index + 1) + 1;
        }
    }
    
    return true;
}

successful_t update_installation(config_t *config, download_buffer_t dlbuffer){
    jsmn_parser parser;
    jsmn_init(&parser);

    length_t required_tokens = jsmn_parse(&parser, dlbuffer.bytes, dlbuffer.length, NULL, 0);
    jsmntok_t *tokens = malloc(sizeof(jsmntok_t) * required_tokens);

    jsmn_init(&parser);
    int parse_error = jsmn_parse(&parser, dlbuffer.bytes, dlbuffer.length, tokens, required_tokens);

    if(parse_error < 0 || required_tokens != parse_error){
        redprintf("Failed to parse downloaded JSON, %s\n", jsmn_helper_parse_fail_reason(parse_error));
        free(tokens);
        return false;
    }

    if(!jsmn_helper_get_object(dlbuffer.bytes, tokens, required_tokens, 0)){
        free(tokens);
        return false;
    }

    jsmntok_t master_object_token = tokens[0];

    // Handle each section of configuration
    char key[256];
    length_t section_token_index = 1;
    length_t total_sections = master_object_token.size;

    jsmntok_t maybe_last_update;
    memset(&maybe_last_update, 0, sizeof(jsmntok_t));

    for(length_t section = 0; section != total_sections; section++){
        if(!jsmn_helper_get_string(dlbuffer.bytes, tokens, required_tokens, section_token_index++, key, sizeof(key))){
            redprintf("Failed to process downloaded JSON\n");
            free(tokens);
            return false;
        }

        if(strcmp(key, "adept.stash") == 0){
            // Stash Header
            stash_header_t stash_header;
            memset(&stash_header, 0, sizeof(stash_header_t));

            if(!process_adept_stash_value(config, &stash_header, dlbuffer, tokens, required_tokens, section_token_index)){
                redprintf("Failed to handle value while processing stash header of downloaded JSON file\n");
                return false;
            }

            if(strcmp(stash_header.latest_compiler_version, "2.4-indev") != 0 && config->show_new_compiler_available){
                blueprintf("\nNEWS: A newer version of Adept is available!\n");
                printf("    (Visit https://github.com/IsaacShelton/Adept for more information)\n\n");
                return true;
            }

            // Free potentially allocated values
            free(stash_header.latest_compiler_version);
        } else {
            // Ignore unknown keys
        }

        section_token_index += jsmn_helper_subtoken_count(tokens, section_token_index);
    }

    free(tokens);
    return true;
}

successful_t process_adept_stash_value(config_t *config, stash_header_t *out_header, download_buffer_t dlbuffer, jsmntok_t *tokens, length_t num_tokens, length_t index){
    char content[1024];
    length_t max_content = 1024;
    long long integer_value;

    if(!jsmn_helper_get_object(dlbuffer.bytes, tokens, num_tokens, index)) return false;

    jsmntok_t main_object_token = tokens[index++];

    for(length_t i = 0; i != main_object_token.size; i++){
        if(!jsmn_helper_get_string(dlbuffer.bytes, tokens, num_tokens, index, content, max_content)) return false;

        if(strcmp(content, "minSpecVersion") == 0){
            if(!jsmn_helper_get_integer(dlbuffer.bytes, tokens, num_tokens, ++index, &integer_value)) return false;
            out_header->min_spec_version = integer_value;
            index++;
        } else if(strcmp(content, "lastUpdated") == 0){
            if(!jsmn_helper_get_integer(dlbuffer.bytes, tokens, num_tokens, ++index, &integer_value)) return false;
            out_header->last_updated = integer_value;
            index++;
        } else if(strcmp(content, "latestCompilerVersion") == 0){
            if(!jsmn_helper_get_string(dlbuffer.bytes, tokens, num_tokens, ++index, content, max_content)) return false;
            out_header->latest_compiler_version = strclone(content);
            index++;
        } else {
            // Ignore unknown keys
            index += jsmn_helper_subtoken_count(tokens, index + 1) + 1;
        }
    }

    return true;
}
