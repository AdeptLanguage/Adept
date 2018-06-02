
#ifndef BACKEND_H
#define BACKEND_H

/*
    ================================= backend.h ================================
    Module for exporting intermediate representation given some backend

    NOTE: 'backend_whatever.h' files are for mapping a backend to this module.
    ----------------------------------------------------------------------------
*/

#include "ir.h"
#include "ground.h"
#include "object.h"
#include "compiler.h"

// Possible backends
#define BACKEND_NONE 0x00000000
#define BACKEND_LLVM 0x00000001

// ---------------- ir_export ----------------
// Exports intermediate representation given
// some backend.
int ir_export(compiler_t *compiler, object_t *object, unsigned int backend);

#endif // BACKED_H
