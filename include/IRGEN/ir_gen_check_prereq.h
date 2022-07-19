
#ifndef _ISAAC_IR_GEN_CHECK_PREREQ_H
#define _ISAAC_IR_GEN_CHECK_PREREQ_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    =========================== ir_gen_check_prereq.h ===========================
    Module for checking polymorphic prerequisites
    -----------------------------------------------------------------------------
*/

#include "DRVR/compiler.h"
#include "DRVR/object.h"
#include "AST/ast_type_lean.h"
#include "UTIL/ground.h"

// (sorted)
enum special_prereq {
    SPECIAL_PREREQ_ASSIGN,
    SPECIAL_PREREQ_NUMBER,
    SPECIAL_PREREQ_POD,
    SPECIAL_PREREQ_PRIMITIVE,
    SPECIAL_PREREQ_SIGNED,
    SPECIAL_PREREQ_STRUCT,
    SPECIAL_PREREQ_UNSIGNED,
    NUM_SPECIAL_PREREQ,
};

// (sorted)
static const char *global_special_prerequisites[] = {
    "__assign__", "__number__", "__pod__", "__primitive__", "__signed__", "__struct__", "__unsigned__"
};

static_assert(
    NUM_ITEMS(global_special_prerequisites) == NUM_SPECIAL_PREREQ,
    "Array of special prerequisite keywords must be the same length as the number of special prerequisites"
);

// ---------------- ir_gen_check_prereq ----------------
// Sets 'out_meets' to whether a certain 'concrete_type'
// meets the polymorphic prerequisite
// Returns SUCCESS if nothing went wrong
errorcode_t ir_gen_check_prereq(
    compiler_t *compiler,
    object_t *object,
    ast_type_t *concrete_type_view,
    enum special_prereq special_prereq,
    bool *out_meets
);

// ---------------- is_special_prerequisite ----------------
// Returns whether a prerequisite is a "special built-in" prerequisite
// If so and 'out_special_prerequisite' isn't NULL, the corresponding
// SPECIAL_PREREQ_* value will be stored in 'out_special_prerequisite'
// NOTE: Returns false for NULL pointer
bool is_special_prerequisite(maybe_null_weak_cstr_t prerequisite_name, enum special_prereq *out_special_prerequisite);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_IR_GEN_CHECK_PREREQ_H
