
#include "DRVR/object.h"

void object_init_ast(object_t *object, unsigned int cross_compile_for){
    ast_init(&object->ast, cross_compile_for);
    object->compilation_stage = COMPILATION_STAGE_AST;
}
