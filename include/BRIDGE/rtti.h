
#ifndef RTTI_H
#define RTTI_H

#include "UTIL/ground.h"
#include "AST/ast_type.h"
#include "IRGEN/ir_builder.h"

// ---------------- rtti_for ----------------
// NOTE: Returns NULL on failure
ir_value_t* rtti_for(ir_builder_t *builder, ast_type_t *target, source_t sourceOnFailure);

#endif // RTTI_H