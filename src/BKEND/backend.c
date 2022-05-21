
#include "BKEND/backend.h"

#include "BKEND/backend_c.h"
#include "BKEND/backend_llvm.h"
#include "DRVR/compiler.h"
#include "DRVR/object.h"
#include "UTIL/color.h"
#include "UTIL/ground.h"

errorcode_t ir_export(compiler_t *compiler, object_t *object, enum ir_export_backend backend){
    switch(backend){
    case BACKEND_NONE:
        internalerrorprintf("No backend specified\n");
        return FAILURE;
    case BACKEND_LLVM:
        return ir_to_llvm(compiler, object);
    case BACKEND_C:
        return ir_to_c(compiler, object);
    default:
        internalerrorprintf("IR Export failed due to unrecognized backend\n");
        return FAILURE;
    }
}
