
#ifndef BACKEND_LLVM_H
#define BACKEND_LLVM_H

/*
    ============================== backend_llvm.h ==============================
    Module for exporting intermediate representation via LLVM

    NOTE: Corresponds to ir_to_llvm.c & ir_to_llvm.h
    ----------------------------------------------------------------------------
*/

#include "object.h"
#include "compiler.h"

// ---------------- ir_to_llvm ----------------
// Invokes the LLVM backend
int ir_to_llvm(compiler_t *compiler, object_t *object);

#endif // BACKEND_LLVM_H
