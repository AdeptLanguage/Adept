
#include <stddef.h>

#include "AST/ast_type.h"
#include "BRIDGE/rtti_collector.h"
#include "BRIDGEIR/rtti.h"
#include "DRVR/object.h"
#include "IR/ir.h"
#include "IR/ir_module.h"
#include "IR/ir_type.h"
#include "IR/ir_value.h"
#include "IRGEN/ir_build_instr.h"
#include "IRGEN/ir_build_literal.h"
#include "IRGEN/ir_builder.h"
#include "UTIL/color.h"
#include "UTIL/datatypes.h"
#include "UTIL/ground.h"

ir_value_t* rtti_for(ir_builder_t *builder, ast_type_t *ast_type, source_t source_on_failure){
    // NOTE: Returns NULL on failure

    // Mention AST type to RTTI collector if applicable
    rtti_collector_t *rtti_collector = builder->object->ir_module.rtti_collector;

    if(rtti_collector){
        rtti_collector_mention(rtti_collector, ast_type);
    }

    maybe_index_t __types__ = ir_builder___types__(builder, source_on_failure);
    if(__types__ < 0) return NULL;

    ir_value_t *placeholder_index = build_literal_usize(builder->pool, /* Will be filled in later */ 0);

    ir_type_t *rtti_array_type = builder->object->ir_module.globals[__types__].type;
    ir_value_t *rtti_array = build_load(builder, build_gvarptr(builder, ir_type_make_pointer_to(builder->pool, rtti_array_type, false), __types__), NULL_SOURCE);

    // Register necessary RTTI relocation
    ir_builder_add_rtti_relocation(builder, ast_type_str(ast_type), ((adept_usize*) placeholder_index->extra), source_on_failure);

    // Return value `__types__[placeholder]`
    return build_load(builder, build_array_access(builder, rtti_array, placeholder_index, NULL_SOURCE), NULL_SOURCE);
}

errorcode_t rtti_resolve(rtti_table_t *rtti_table, rtti_relocation_t *relocation){
    strong_cstr_t human_notation = relocation->human_notation;
    maybe_index_t index = rtti_table_find_index(rtti_table, human_notation);

    if(index < 0){
        internalerrorprintf("rtti_resolve() - Failed to find info for type '%s', which should exist\n", human_notation);
        return FAILURE;
    }

    *relocation->id_ref = index;
    return SUCCESS;
}
