
#ifndef _ISAAC_BACKEND_LLVM_H
#define _ISAAC_BACKEND_LLVM_H

/*
    ============================== backend_llvm.h ==============================
    Module for exporting intermediate representation via LLVM

    NOTE: Corresponds to ir_to_llvm.c & ir_to_llvm.h
    ----------------------------------------------------------------------------
*/

#include "DRVR/object.h"
#include "DRVR/compiler.h"

// ---------------- ir_to_llvm ----------------
// Invokes the LLVM backend
errorcode_t ir_to_llvm(compiler_t *compiler, object_t *object);

#endif // _ISAAC_BACKEND_LLVM_H
