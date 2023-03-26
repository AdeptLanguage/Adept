
#ifndef _ISAAC_CONFIG_H
#define _ISAAC_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "UTIL/ground.h"

enum update_schedule {
    UPDATE_SCHEDULE_NEVER,
    UPDATE_SCHEDULE_EVERYTIME,
    UPDATE_SCHEDULE_HOURLY,
    UPDATE_SCHEDULE_DAILY,
    UPDATE_SCHEDULE_WEEKLY,
    UPDATE_SCHEDULE_MONTHLY,
    UPDATE_SCHEDULE_YEARLY
};

typedef struct {
    strong_cstr_t cainfo_file;

    bool has;
    enum update_schedule update;
    long long last_updated;
    strong_cstr_t stash;
    bool show_new_compiler_available;
    bool show_checking_for_updates_message;
} config_t;

typedef struct {
    long long min_spec_version;
    long long last_updated;
    strong_cstr_t latest_compiler_version;
} stash_header_t;

void config_prepare(config_t *config, strong_cstr_t cainfo_file);
void config_free(config_t *config);
successful_t config_read(config_t *config, weak_cstr_t filename, bool no_update, weak_cstr_t *out_warning);

#ifdef ADEPT_ENABLE_PACKAGE_MANAGER
    #define JSMN_HEADER
    #include "UTIL/jsmn.h"
    #include "UTIL/jsmn_helper.h"

    void try_update_installation(config_t *config, weak_cstr_t filename, jsmnh_buffer_t *optional_config_fulltext, jsmntok_t *optional_last_update);
#endif

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_CONFIG_H
