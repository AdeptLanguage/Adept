
#include "AST/ast_layout.h"
#include "AST/POLY/ast_resolve.h"
#include "IRGEN/ir_autogen.h"
#include "IRGEN/ir_build_instr.h"
#include "IRGEN/ir_gen_type.h"

errorcode_t ir_autogen_for_children_of_struct_like(
    ir_builder_t *builder,
    enum ir_autogen_action_kind action_kind,
    ast_elem_t *struct_like_elem,
    ast_type_t *param_ast_type
){
    assert(struct_like_elem->id == AST_ELEM_BASE || struct_like_elem->id == AST_ELEM_GENERIC_BASE);

    bool is_polymorphic = struct_like_elem->id == AST_ELEM_GENERIC_BASE;
    
    ast_t *ast = &builder->object->ast;
    rtti_collector_t *rtti_collector = builder->object->ir_module.rtti_collector;

    ast_composite_t *composite = NULL;

    if(is_polymorphic){
        ast_elem_generic_base_t *generic_base = (ast_elem_generic_base_t*) struct_like_elem;
        composite = (ast_composite_t*) ast_poly_composite_find_exact_from_elem(ast, generic_base);
    } else {
        weak_cstr_t composite_name = ((ast_elem_base_t*) struct_like_elem)->base;
        composite = ast_composite_find_exact(ast, composite_name);
    }

    // Don't bother with composites that don't exist
    if(composite == NULL) return FAILURE;

    // Don't handle children for complex composite types
    if(!ast_layout_is_simple_struct(&composite->layout)) return SUCCESS;

    // Substitution Catalog
    ast_poly_catalog_t catalog;
    ast_poly_catalog_init(&catalog);

    if(is_polymorphic){
        ast_poly_composite_t *template = (ast_poly_composite_t*) composite;
        ast_elem_generic_base_t *generic_base = (ast_elem_generic_base_t*) struct_like_elem;

        if(template->generics_length != generic_base->generics_length){
            internalerrorprintf("handle_children_dereference() - Polymorphic struct '%s' type parameter length mismatch when generating child deference!\n", generic_base->name);
            ast_poly_catalog_free(&catalog);
            return ALT_FAILURE;
        }

        ast_poly_catalog_add_types(&catalog, template->generics, generic_base->generics, template->generics_length);
    }

    ast_layout_skeleton_t *skeleton = &composite->layout.skeleton;
    length_t field_count = ast_simple_field_map_get_count(&composite->layout.field_map);

    // Capture snapshots for if we need to backtrack
    ir_pool_snapshot_t full_pool_snapshot = ir_pool_snapshot_capture(builder->pool);
    ir_instrs_snapshot_t full_instrs_snapshot = ir_instrs_snapshot_capture(builder);

    for(length_t field_i = 0; field_i != field_count; field_i++){
        ast_type_t *ast_unresolved_field_type = ast_layout_skeleton_get_type_at_index(skeleton, field_i);

        ast_type_t ast_field_type;
        if(ast_resolve_type_polymorphs(builder->compiler, rtti_collector, &catalog, ast_unresolved_field_type, &ast_field_type)){
            return ALT_FAILURE;
        }

        if(ir_autogen_for_child_of_struct(builder, action_kind, &ast_field_type, field_i, param_ast_type, composite->source, ast_unresolved_field_type->source) == ALT_FAILURE){
            ir_instrs_snapshot_restore(builder, &full_instrs_snapshot);
            ir_pool_snapshot_restore(builder->pool, &full_pool_snapshot);
            ast_poly_catalog_free(&catalog);
            ast_type_free(&ast_field_type);
            return ALT_FAILURE;
        }

        ast_type_free(&ast_field_type);
    }

    ast_poly_catalog_free(&catalog);
    return SUCCESS;
}

errorcode_t ir_autogen_for_child_of_struct(
    ir_builder_t *builder,
    enum ir_autogen_action_kind action_kind,
    ast_type_t *ast_field_type,
    length_t field_i,
    ast_type_t *param_ast_type,
    source_t composite_source,
    source_t field_source
){
    // Capture snapshots for if we need to backtrack
    ir_pool_snapshot_t field_pool_snapshot = ir_pool_snapshot_capture(builder->pool);
    ir_instrs_snapshot_t field_instrs_snapshot = ir_instrs_snapshot_capture(builder);

    errorcode_t field_errorcode = ALT_FAILURE;

    ir_type_t *ir_field_type, *param_ir_type;
    if(
        ir_gen_resolve_type(builder->compiler, builder->object, ast_field_type, &ir_field_type) ||
        ir_gen_resolve_type(builder->compiler, builder->object, param_ast_type, &param_ir_type)
    ){
        field_errorcode = ALT_FAILURE;
        goto end;
    }

    switch(action_kind){
    case IR_AUTOGEN_ACTION_DEFER: {
            ir_value_t *this_ir_value = build_load(builder, build_lvarptr(builder, ir_type_make_pointer_to(builder->pool, param_ir_type, false), 0), param_ast_type->source);
            ir_value_t *ir_field_value = build_member(builder, this_ir_value, field_i, ir_type_make_pointer_to(builder->pool, ir_field_type, false), param_ast_type->source);
            field_errorcode = handle_single_deference(builder, ast_field_type, ir_field_value, composite_source);
        }
        break;
    case IR_AUTOGEN_ACTION_PASS: {
            if(ir_field_type->kind != TYPE_KIND_STRUCTURE && ir_field_type->kind != TYPE_KIND_FIXED_ARRAY){
                field_errorcode = FAILURE;
                goto end;
            }

            ir_value_t *mutable_passed_ir_value = build_lvarptr(builder, ir_type_make_pointer_to(builder->pool, param_ir_type, false), 0);
            ir_value_t *ir_field_reference = build_member(builder, mutable_passed_ir_value, field_i, ir_type_make_pointer_to(builder->pool, ir_field_type, false), field_source);
            field_errorcode = handle_single_pass(builder, ast_field_type, ir_field_reference, NULL_SOURCE);
        }
        break;
    default:
        die("ir_autogen_for_child_of_struct() got unknown action kind\n");
    }

end:
    if(field_errorcode != SUCCESS){
        // Revert recent instructions
        ir_instrs_snapshot_restore(builder, &field_instrs_snapshot);

        // Revert recent pool allocations
        ir_pool_snapshot_restore(builder->pool, &field_pool_snapshot);
    }

    return field_errorcode;
}
