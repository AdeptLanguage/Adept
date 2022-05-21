
#ifndef _ISAAC_BACKEND_H
#define _ISAAC_BACKEND_H

/*
    ================================= backend.h ================================
    Module for exporting intermediate representation given some backend

    NOTE: 'backend_whatever.h' files are for mapping a backend to this module.
    ----------------------------------------------------------------------------
*/

#include "DRVR/compiler.h"
#include "DRVR/object.h"
#include "UTIL/ground.h"

// Possible backends
enum ir_export_backend {
    BACKEND_NONE,
    BACKEND_LLVM,
    BACKEND_C,
};

// ---------------- ir_export ----------------
// Exports intermediate representation given
// some backend.
errorcode_t ir_export(compiler_t *compiler, object_t *object, enum ir_export_backend backend);

#endif // _ISAAC_BACKEND_H
