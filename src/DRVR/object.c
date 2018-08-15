
#include "DRVR/object.h"

void object_init_ast(object_t *object){
    ast_init(&object->ast);
    object->compilation_stage = COMPILATION_STAGE_AST;
}
