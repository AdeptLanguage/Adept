
#ifdef ADEPT_ENABLE_PACKAGE_MANAGER

#include "UTIL/stash.h"

successful_t adept_install(compiler_t *compiler, weak_cstr_t identifier){
    return false;
}

successful_t adept_uninstall(compiler_t *compiler, weak_cstr_t identifier){
    return false;
}

successful_t adept_info(compiler_t *compiler, weak_cstr_t identifier){
    config_t *config = compiler_get_config();
    return false;
}

#endif