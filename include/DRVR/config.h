
#ifndef _ISAAC_CONFIG_H
#define _ISAAC_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#define JSMN_HEADER
#include "UTIL/jsmn.h"
#include "UTIL/ground.h"
#include "UTIL/download.h"

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
    bool has;
    enum update_schedule update;
    long long last_updated;
    strong_cstr_t stash;
    bool show_new_compiler_available;
} config_t;

typedef struct {
    long long min_spec_version;
    long long last_updated;
    strong_cstr_t latest_compiler_version;
} stash_header_t;

void config_prepare(config_t *config);
void config_free(config_t *config);
successful_t config_read(config_t *config, weak_cstr_t filename, weak_cstr_t *out_warning);
successful_t config_update_last_updated(config_t *config, weak_cstr_t filename, weak_cstr_t buffer, length_t buffer_length, jsmntok_t last_update);
successful_t config_read_adept_config_value(config_t *config, weak_cstr_t buffer, jsmntok_t *tokens, length_t num_tokens, length_t index, jsmntok_t *out_maybe_last_update);

successful_t update_installation(config_t *config, download_buffer_t dlbuffer);
successful_t process_adept_stash_value(config_t *config, stash_header_t *out_header, download_buffer_t dlbuffer, jsmntok_t *tokens, length_t num_tokens, length_t index);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_CONFIG_H
