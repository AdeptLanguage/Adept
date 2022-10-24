
#include "DRVR/object.h"

void object_init_ast(object_t *object, unsigned int cross_compile_for){
    ast_init(&object->ast, cross_compile_for);
    object->compilation_stage = COMPILATION_STAGE_AST;
}

#ifndef ADEPT_INSIGHT_BUILD
void object_create_module(object_t *object){
    ast_t *ast = &object->ast;
    ir_module_t *module = &object->ir_module;

    // Initialize module
    ir_module_init(module, ast->funcs_length, ast->globals_length, ast->funcs_length + ast->func_aliases_length + 32);

    // Advance compilation stage
    object->compilation_stage = COMPILATION_STAGE_IR_MODULE;
}
#endif
