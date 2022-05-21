
#ifndef ENABLE_DEBUG_FEATURES
#error This file should never be included in release builds
#else

#include "AST/ast.h"
#include "AST/ast_expr.h"
#include "LEX/token.h"
#include "UTIL/color.h"
#include "UTIL/ground.h"
#include "DBG/debug.h"
#include "DRVR/compiler.h"
#include "IR/ir.h"
#include "IR/ir_dump.h"
#include "IR/ir_lowering.h"
#include "IRGEN/ir_builder.h"

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
    case DEBUG_SIGNAL_AT_OUT:                OPTIONAL_NOTIF_MACRO(COMPILER_DEBUG_STAGES, "STAGE: OBJECT OUT"); break;
    case DEBUG_SIGNAL_AT_LINKING:            OPTIONAL_NOTIF_MACRO(COMPILER_DEBUG_STAGES, "STAGE: LINKING");    break;
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

#endif // ENABLE_DEBUG_FEATURES
