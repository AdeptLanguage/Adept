
#if !defined(_ISAAC_STASH_H) && defined(ADEPT_ENABLE_PACKAGE_MANAGER)
#define _ISAAC_STASH_H

#include "DRVR/config.h"

successful_t adept_install(config_t *config, weak_cstr_t root, weak_cstr_t identifier);
successful_t adept_install_stash(config_t *config, weak_cstr_t root, char *bytes, length_t length);
maybe_null_strong_cstr_t adept_locate_package(weak_cstr_t identifier, char *buffer, length_t length);
successful_t adept_uninstall(config_t *config, weak_cstr_t identifier);
successful_t adept_info(config_t *config, weak_cstr_t identifier);

#endif // _ISAAC_STASH_H
