
#if !defined(STASH_H) && defined(ADEPT_ENABLE_PACKAGE_MANAGER)
#define STASH_H

#include "UTIL/ground.h"
#include "DRVR/compiler.h"

successful_t adept_install(compiler_t *compiler, weak_cstr_t identifier);
successful_t adept_uninstall(compiler_t *compiler, weak_cstr_t identifier);
successful_t adept_info(compiler_t *compiler, weak_cstr_t identifier);

#endif // STASH_H
