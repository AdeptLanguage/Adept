
#ifndef _ISAAC_BACKEND_C_H
#define _ISAAC_BACKEND_C_H

/*
    ================================ backend_c.h ================================
    Module for exporting intermediate representation via C

    NOTE: Corresponds to ir_to_c.c & ir_to_c.h
    -----------------------------------------------------------------------------
*/

#include "DRVR/object.h"
#include "DRVR/compiler.h"

// ---------------- ir_to_c ----------------
// Invokes the LLVM backend
errorcode_t ir_to_c(compiler_t *compiler, object_t *object);

#endif // _ISAAC_BACKEND_C_H
