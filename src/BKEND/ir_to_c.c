
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AST/ast.h"
#include "BKEND/ir_to_c.h"
#include "DRVR/compiler.h"
#include "DBG/debug.h"
#include "DRVR/object.h"
#include "IR/ir.h"
#include "UTIL/color.h"
#include "UTIL/filename.h"
#include "UTIL/ground.h"
#include "UTIL/list.h"
#include "UTIL/string.h"
#include "UTIL/string_builder.h"
#include "UTIL/util.h"

errorcode_t ir_to_c(compiler_t *compiler, object_t *object){
    (void) compiler;
    (void) object;

    redprintf("The C backend is not implemented yet\n");
    return FAILURE;
}
