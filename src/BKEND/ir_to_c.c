
#include "BKEND/ir_to_c.h"
#include "DRVR/compiler.h"
#include "DRVR/object.h"
#include "UTIL/color.h"
#include "UTIL/ground.h"

errorcode_t ir_to_c(compiler_t *compiler, object_t *object){
    (void) compiler;
    (void) object;

    redprintf("The C backend is not implemented yet\n");
    return FAILURE;
}
