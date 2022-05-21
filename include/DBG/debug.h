
#ifndef _ISAAC_DEBUG_H
#define _ISAAC_DEBUG_H

/*
    ================================== debug.h =================================
    Module for debugging small chunks of code and running tests.
    ----------------------------------------------------------------------------
*/

// Possible signals to pass to this module
enum {
    DEBUG_SIGNAL_AT_STAGE_ARGS_AND_LEX, // data = NULL
    DEBUG_SIGNAL_AT_STAGE_PARSE,        // data = NULL
    DEBUG_SIGNAL_AT_AST_DUMP,           // data = ast_t*
    DEBUG_SIGNAL_AT_INFERENCE,          // data = NULL
    DEBUG_SIGNAL_AT_INFER_DUMP,         // data = ast_t*
    DEBUG_SIGNAL_AT_ASSEMBLY,           // data = NULL
    DEBUG_SIGNAL_AT_IR_MODULE_DUMP,     // data = ir_module_t*
    DEBUG_SIGNAL_AT_EXPORT,             // data = NULL
    DEBUG_SIGNAL_AT_OUT,                // data = NULL
    DEBUG_SIGNAL_AT_LINKING,            // data = NULL
};

#ifdef ENABLE_DEBUG_FEATURES
    #include "DRVR/compiler.h"

    // ---------------- debug_signal(compiler, signal, data) ----------------
    // Sends a signal to this debugging module
    #define debug_signal(a, b, c) handle_debug_signal(a, b, c);

    // ---------------- handle_debug_signal ----------------
    // Implementation of 'debug_signal'
    void handle_debug_signal(compiler_t *compiler, unsigned int sig, void *data);
#else
    #define debug_signal(a, b, c) ((void) 0)
#endif // ENABLE_DEBUG_FEATURES

#endif // _ISAAC_DEBUG_H
