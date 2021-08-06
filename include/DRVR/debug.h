
#ifndef _ISAAC_DEBUG_H
#define _ISAAC_DEBUG_H

/*
    ================================== debug.h =================================
    Module for debugging small chunks of code and running tests.
    ----------------------------------------------------------------------------
*/

// Possible signals to pass to this module
#define DEBUG_SIGNAL_AT_STAGE_ARGS_AND_LEX 0x01 // data = NULL
#define DEBUG_SIGNAL_AT_STAGE_PARSE        0x02 // data = NULL
#define DEBUG_SIGNAL_AT_AST_DUMP           0x03 // data = ast_t*
#define DEBUG_SIGNAL_AT_INFERENCE          0x04 // data = NULL
#define DEBUG_SIGNAL_AT_INFER_DUMP         0x05 // data = ast_t*
#define DEBUG_SIGNAL_AT_ASSEMBLY           0x06 // data = NULL
#define DEBUG_SIGNAL_AT_IR_MODULE_DUMP     0x07 // data = ir_module_t*
#define DEBUG_SIGNAL_AT_EXPORT             0x08 // data = NULL
#define DEBUG_SIGNAL_AT_OUT                0x09 // data = NULL
#define DEBUG_SIGNAL_AT_LINKING            0x0A // data = NULL

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
