
#include <stddef.h>

#include "AST/ast_type.h"
#include "BRIDGE/rtti.h"
#include "BRIDGE/type_table.h"
#include "DRVR/object.h"
#include "IR/ir.h"
#include "IR/ir_type.h"
#include "IR/ir_value.h"
#include "IRGEN/ir_builder.h"
#include "UTIL/color.h"
#include "UTIL/datatypes.h"
#include "UTIL/ground.h"

ir_value_t* rtti_for(ir_builder_t *builder, ast_type_t *ast_type, source_t source_on_failure){
    // NOTE: Returns NULL on failure
    maybe_index_t __types__ = ir_builder___types__(builder, source_on_failure);
    if(__types__ < 0) return NULL;

    ir_value_t *index = build_literal_usize(builder->pool, /* Will be filled in later */ 0);
    if(build_rtti_relocation(builder, ast_type_str(ast_type), ((adept_usize*) index->extra), source_on_failure)) return NULL;

    ir_global_t *global = &builder->object->ir_module.globals[__types__];
    ir_value_t *rtti_array = build_load(builder, build_gvarptr(builder, ir_type_pointer_to(builder->pool, global->type), __types__), NULL_SOURCE);
    return build_load(builder, build_array_access(builder, rtti_array, index, NULL_SOURCE), NULL_SOURCE);
}

errorcode_t rtti_resolve(type_table_t *type_table, rtti_relocation_t *relocation){
    strong_cstr_t human_notation = relocation->human_notation;
    maybe_index_t index = type_table_find(type_table, human_notation);

    if(index == -1){
        internalerrorprintf("rtti_resolve() - Failed to find info for type '%s', which should exist\n", human_notation);
        return FAILURE;
    }

    *relocation->id_ref = index;
    return SUCCESS;
}
