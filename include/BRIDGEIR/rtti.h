
#ifndef _ISAAC_RTTI_H
#define _ISAAC_RTTI_H

#include "AST/ast_type_lean.h"
#include "BRIDGE/rtti_collector.h"
#include "BRIDGEIR/rtti_table.h"
#include "IR/ir.h"
#include "IR/ir_value.h"
#include "IRGEN/ir_builder.h"
#include "UTIL/ground.h"

// ---------------- rtti_for ----------------
// NOTE: Returns NULL on failure
ir_value_t* rtti_for(ir_builder_t *builder, ast_type_t *ast_type, source_t source_on_failure);

// ---------------- rtti_resolve ----------------
// Resolves a single RTTI relocation
// NOTE: Used to fill in requested indices for the __types__ runtime array
errorcode_t rtti_resolve(rtti_table_t *rtti_table, rtti_relocation_t *relocation);

#endif // _ISAAC_RTTI_H
