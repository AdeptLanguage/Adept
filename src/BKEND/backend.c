
#include "UTIL/color.h"
#include "UTIL/ground.h"
#include "BKEND/backend.h"
#include "BKEND/backend_llvm.h"

errorcode_t ir_export(compiler_t *compiler, object_t *object, unsigned int backend){
    switch(backend){
    case BACKEND_NONE:
        redprintf("INTERNAL ERROR: No backend specified\n");
        return FAILURE;
    case BACKEND_LLVM:
        return ir_to_llvm(compiler, object);
    default:
        redprintf("INTERNAL ERROR: IR Export failed due to unrecognized backend\n");
        return FAILURE;
    }
}
