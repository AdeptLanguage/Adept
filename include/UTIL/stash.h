
#if !defined(_ISAAC_STASH_H) && defined(ADEPT_ENABLE_PACKAGE_MANAGER)
#define _ISAAC_STASH_H

#include "DRVR/config.h"
#include "UTIL/ground.h"
#include "DRVR/compiler.h"

successful_t adept_install(config_t *config, weak_cstr_t root, weak_cstr_t identifier);
successful_t adept_install_stash(config_t *config, weak_cstr_t root, char *bytes, length_t length);
successful_t adept_perform_procedure_command(char *buffer, jsmntok_t *tokens, length_t num_tokens, length_t token_index, weak_cstr_t root, maybe_null_weak_cstr_t host);
maybe_null_strong_cstr_t adept_locate_package(weak_cstr_t identifier, char *buffer, length_t length);
successful_t adept_uninstall(config_t *config, weak_cstr_t identifier);
successful_t adept_info(config_t *config, weak_cstr_t identifier);

#endif // _ISAAC_STASH_H
