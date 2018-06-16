
#ifndef ENABLE_DEBUG_FEATURES
#error This file should never be included in release builds
#endif // ENABLE_DEBUG_FEATURES

#include "ast.h"
#include "color.h"
#include "debug.h"
#include "token.h"
#include "ground.h"
#include "ast_expr.h"
#include "compiler.h"

void handle_debug_signal(compiler_t *compiler, unsigned int sig, void *data){
    #define OPTIONAL_NOTIF_MACRO(optional_trait, message) { \
        if(compiler->debug_traits & optional_trait) puts(message); \
    }

    switch(sig){
    case DEBUG_SIGNAL_AT_STAGE_ARGS_AND_LEX: OPTIONAL_NOTIF_MACRO(COMPILER_DEBUG_STAGES, "STAGE: ARGS & LEX"); break;
    case DEBUG_SIGNAL_AT_STAGE_PARSE:        OPTIONAL_NOTIF_MACRO(COMPILER_DEBUG_STAGES, "STAGE: PARSE");      break;
    case DEBUG_SIGNAL_AT_INFERENCE:          OPTIONAL_NOTIF_MACRO(COMPILER_DEBUG_STAGES, "STAGE: INFERENCE");  break;
    case DEBUG_SIGNAL_AT_ASSEMBLY:           OPTIONAL_NOTIF_MACRO(COMPILER_DEBUG_STAGES, "STAGE: ASSEMBLY");   break;
    case DEBUG_SIGNAL_AT_EXPORT:             OPTIONAL_NOTIF_MACRO(COMPILER_DEBUG_STAGES, "STAGE: EXPORT");     break;
    case DEBUG_SIGNAL_AT_AST_DUMP:
        if(compiler->debug_traits & COMPILER_DEBUG_DUMP) ast_dump((ast_t*) data, "ast.txt");
        break;
    case DEBUG_SIGNAL_AT_INFER_DUMP:
        if(compiler->debug_traits & COMPILER_DEBUG_DUMP) ast_dump((ast_t*) data, "infer.txt");
        break;
    case DEBUG_SIGNAL_AT_IR_MODULE_DUMP:
        if(compiler->debug_traits & COMPILER_DEBUG_DUMP) ir_module_dump((ir_module_t*) data, "ir.txt");
        break;
    default:
        printf("Unknown debug signal %08X\n", (int) sig);
    }

    #undef OPTIONAL_NOTIF_MACRO
}

#define test(a) if(!(a)) whiteprintf("!!! DEBUG TEST FAILED '#a' on line %d\n", __LINE__);

void run_debug_tests(){
    bool ok = true;
    ok = ok && rep_table_tests(); // Coverage test of representation tables
    ok = ok && ast_expr_tests();  // Coverage test of ast_expr_* functions

    if(!ok) whiteprintf("!!! DEBUG TESTS FAILED !!!\n");
}

bool ast_expr_tests(){
    // NOTE: Tests expressions on ast_expr_clone(ast_expr_t* expr)
    // NOTE: Returns whether ok

    bool ok = true;
    whiteprintf("Running Test: ast_expr_tests()\n");

    ast_expr_t *clone;

    #define MACRO_PRIMITIVE_TEST(name, expr_type, expr) { \
        ast_expr_byte_t *_p = malloc(sizeof(expr_type)); \
        _p->id = expr; \
        _p->source.index = 0; \
        _p->source.object_index = 0; \
        _p->value = 0; \
        clone = ast_expr_clone((ast_expr_t*) _p); \
        if(clone == NULL) { whiteprintf("!!! FAILURE Cloning %s expression\n", name); ok = false; } \
        else { ast_expr_free(clone); free(clone); } \
        free(_p); \
    }

    MACRO_PRIMITIVE_TEST("byte", ast_expr_byte_t, EXPR_BYTE);
    MACRO_PRIMITIVE_TEST("ubyte", ast_expr_ubyte_t, EXPR_UBYTE);
    MACRO_PRIMITIVE_TEST("short", ast_expr_short_t, EXPR_SHORT);
    MACRO_PRIMITIVE_TEST("ushort", ast_expr_ushort_t, EXPR_USHORT);
    MACRO_PRIMITIVE_TEST("int", ast_expr_int_t, EXPR_INT);
    MACRO_PRIMITIVE_TEST("uint", ast_expr_uint_t, EXPR_UINT);
    MACRO_PRIMITIVE_TEST("long", ast_expr_long_t, EXPR_LONG);
    MACRO_PRIMITIVE_TEST("ulong", ast_expr_ulong_t, EXPR_ULONG);
    MACRO_PRIMITIVE_TEST("float", ast_expr_float_t, EXPR_FLOAT);
    MACRO_PRIMITIVE_TEST("double", ast_expr_double_t, EXPR_DOUBLE);
    MACRO_PRIMITIVE_TEST("generic int", ast_expr_generic_int_t, EXPR_GENERIC_INT);
    MACRO_PRIMITIVE_TEST("generic float", ast_expr_generic_float_t, EXPR_GENERIC_FLOAT);

    #undef MACRO_PRIMITIVE_TEST

    if(!ok) whiteprintf("!!! FAILURE ast_expr_tests()\n");
    return ok;
}

bool rep_table_tests(){
    // NOTE: Tests token and expression representation tables
    // NOTE: Returns ok if it successfully doesn't crash

    char tmp[256];
    whiteprintf("Running Test: rep_table_tests()\n");

    for(length_t i = 0; i != MAX_LEX_TOKEN + 1; i++){
        strcpy(tmp, global_token_name_table[i]);
        //puts(tmp); // Optionally print for introspection
    }

    for(length_t i = 0; i != MAX_AST_EXPR + 1; i++) {
        strcpy(tmp, global_expression_rep_table[i]);
        //puts(tmp); // Optionally print for introspection
    }

    return true;
}
