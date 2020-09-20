
#ifndef _ISAAC_BACKEND_H
#define _ISAAC_BACKEND_H

/*
    ================================= backend.h ================================
    Module for exporting intermediate representation given some backend

    NOTE: 'backend_whatever.h' files are for mapping a backend to this module.
    ----------------------------------------------------------------------------
*/

#include "IR/ir.h"
#include "UTIL/ground.h"
#include "DRVR/object.h"
#include "DRVR/compiler.h"

// Possible backends
#define BACKEND_NONE 0x00000000
#define BACKEND_LLVM 0x00000001

// ---------------- ir_export ----------------
// Exports intermediate representation given
// some backend.
errorcode_t ir_export(compiler_t *compiler, object_t *object, unsigned int backend);

#endif // _ISAAC_BACKEND_H
