
#include "IR/ir.h"
#include "IR/ir_type.h"
#include "UTIL/util.h"
#include "UTIL/color.h"
#include "BRIDGE/any.h"
#include "IRGEN/ir_gen_rtti.h"
#include "IRGEN/ir_gen_type.h"

errorcode_t ir_gen__types__(compiler_t *compiler, object_t *object, ir_global_t *ir_global){
    
    // Don't generate RTTI when it's disabled, use null as a placeholder
    if(compiler->traits & COMPILER_NO_TYPEINFO){
        return ir_gen__types__placeholder(object, ir_global);
    }

    ir_module_t *ir_module = &object->ir_module;
    ir_pool_t *pool = &ir_module->pool;

    ir_rtti_types_t rtti_types;
    length_t array_length = object->ast.type_table->length;

    // Fetch IR Types for RTTI
    if(ir_gen_rtti_fetch_rtti_types(ir_module, &rtti_types)) return FAILURE;
    
    // Generate Array Values
    ir_value_t **array_values = ir_gen__types__values(compiler, object, &rtti_types);
    if(array_values == NULL) return FAILURE;

    // Create RTTI Array and set __types__'s initializer to be it
    ir_value_t *rtti_array = build_static_array(pool, rtti_types.any_type_ptr_type, array_values, array_length);
    ir_global->trusted_static_initializer = rtti_array;
    return SUCCESS;
}

errorcode_t ir_gen_rtti_fetch_rtti_types(ir_module_t *ir_module, ir_rtti_types_t *out_rtti_types){
    ir_pool_t *pool = &ir_module->pool;
    ir_type_map_t *type_map = &ir_module->type_map;

    // Fetch IR Types for RTTI
    if(!ir_type_map_find(type_map, "AnyType", &out_rtti_types->any_type_type)
    || !ir_type_map_find(type_map, "AnyStructType", &out_rtti_types->any_struct_type_type)
    || !ir_type_map_find(type_map, "AnyUnionType", &out_rtti_types->any_union_type_type)
    || !ir_type_map_find(type_map, "AnyPtrType", &out_rtti_types->any_ptr_type_type)
    || !ir_type_map_find(type_map, "AnyFuncPtrType", &out_rtti_types->any_funcptr_type_type)
    || !ir_type_map_find(type_map, "AnyFixedArrayType", &out_rtti_types->any_fixed_array_type_type)
    || !ir_type_map_find(type_map, "ubyte", &out_rtti_types->ubyte_ptr_type)){
        internalerrorprintf("Failed to find types used by the runtime type table that should've been injected\n");
        return FAILURE;
    }

    // Turn 'ubyte' type into '*ubyte' type
    out_rtti_types->ubyte_ptr_type = ir_type_pointer_to(pool, out_rtti_types->ubyte_ptr_type);

    // Make '*AnyType' type
    out_rtti_types->any_type_ptr_type = ir_type_pointer_to(pool, out_rtti_types->any_type_type);

    // Fetch 'usize' type
    out_rtti_types->usize_type = ir_module->common.ir_usize;
    return SUCCESS;
}

errorcode_t ir_gen__types__placeholder(object_t *object, ir_global_t *ir_global){
    ir_module_t *ir_module = &object->ir_module;
    ir_pool_t *pool = &ir_module->pool;

    ir_type_t *any_type_type;

    // Fetch 'AnyType' IR Type
    if(!ir_type_map_find(&ir_module->type_map, "AnyType", &any_type_type)){
        internalerrorprintf("Failed to get 'AnyType' which should've been injected\n");
        redprintf("    (when creating null pointer to initialize __types__ because type info was disabled)\n");
        return FAILURE;
    }

    // Construct '**AnyType' null pointer and set it as __types__'s initializer
    ir_type_t *any_type_ptr_ptr_type = ir_type_pointer_to(pool, ir_type_pointer_to(pool, any_type_type));
    ir_global->trusted_static_initializer = build_null_pointer_of_type(pool, any_type_ptr_ptr_type);
    return SUCCESS;
}

ir_value_t **ir_gen__types__values(compiler_t *compiler, object_t *object, ir_rtti_types_t *rtti_types){
    // Returns NULL on failure

    ir_module_t *ir_module = &object->ir_module;
    ir_pool_t *pool = &ir_module->pool;
    type_table_t *type_table = object->ast.type_table;
    length_t array_length = type_table->length;

    ir_value_t **array_values = ir_pool_alloc(pool, sizeof(ir_value_t*) * type_table->length);
    
    // Preparation for each array value
    for(length_t i = 0; i != array_length; i++){
        // Resolve AST type to IR type for each type table entry
        if(ir_gen_resolve_type(compiler, object, &type_table->entries[i].ast_type, &type_table->entries[i].ir_type)){
            return NULL;
        }

        // Create foundation for each array value that is of the correct AnyType variant
        ir_type_t *any_type_variant;

        switch(type_table->entries[i].ir_type->kind){
        case TYPE_KIND_POINTER:
            any_type_variant = rtti_types->any_ptr_type_type;
            break;
        case TYPE_KIND_STRUCTURE:
            any_type_variant = rtti_types->any_struct_type_type;
            break;
        case TYPE_KIND_UNION:
            any_type_variant = rtti_types->any_union_type_type;
            break;
        case TYPE_KIND_FUNCPTR:
            any_type_variant = rtti_types->any_funcptr_type_type;
            break;
        case TYPE_KIND_FIXED_ARRAY:
            any_type_variant = rtti_types->any_fixed_array_type_type;
            break;
        default:
            any_type_variant = rtti_types->any_type_type;
        }

        // Create empty anonymous global variable of the correct AnyType variant for the entry
        array_values[i] = build_anon_global(ir_module, any_type_variant, true);
    }

    // TODO: CLEANUP: Cleanup this messy code
    for(length_t i = 0; i != type_table->length; i++){
        ir_type_t *initializer_type;
        ir_value_t **initializer_members;
        length_t initializer_members_length;
        unsigned int any_type_kind_id = ANY_TYPE_KIND_VOID;
        unsigned int type_type_kind = type_table->entries[i].ir_type->kind;

        switch(type_type_kind){
        case TYPE_KIND_POINTER: {
                /* struct AnyPtrType(kind AnyTypeKind, name *ubyte, is_alias bool, size usize, subtype *AnyType) */

                initializer_members = ir_pool_alloc(pool, sizeof(ir_value_t*) * 5);
                initializer_members_length = 5;

                maybe_index_t subtype_index = -1;

                // HACK: We really shouldn't be doing this
                if(type_table->entries[i].ast_type.elements_length > 1){
                    ast_type_t dereferenced = ast_type_clone(&type_table->entries[i].ast_type);
                    
                    ast_type_dereference(&dereferenced);

                    char *dereferenced_name = ast_type_str(&dereferenced);
                    subtype_index = type_table_find(type_table, dereferenced_name);
                    ast_type_free(&dereferenced);
                    free(dereferenced_name);
                }

                if(subtype_index == -1){
                    ir_value_t *null_pointer = build_null_pointer_of_type(pool, rtti_types->any_type_ptr_type);
                    initializer_members[4] = null_pointer; // subtype
                } else {
                    initializer_members[4] = build_const_bitcast(pool, array_values[subtype_index], rtti_types->any_type_ptr_type); // subtype
                }

                initializer_type = rtti_types->any_ptr_type_type;
            }
            break;
        case TYPE_KIND_STRUCTURE: case TYPE_KIND_UNION: {
                /* struct AnyStructType (kind AnyTypeKind, name *ubyte, is_alias bool, size usize, members **AnyType, length usize, offsets *usize, member_names **ubyte, is_packed bool) */
                /* struct AnyUnionType (kind AnyTypeKind, name *ubyte, is_alias bool, size usize, members **AnyType, length usize, member_names **ubyte) */

                ir_type_extra_composite_t *extra = (ir_type_extra_composite_t*) type_table->entries[i].ir_type->extra;
                initializer_members_length = type_type_kind == TYPE_KIND_STRUCTURE ? 9 : 7;
                initializer_members = ir_pool_alloc(pool, sizeof(ir_value_t*) * initializer_members_length);

                ir_value_t **members = ir_pool_alloc(pool, sizeof(ir_value_t*) * extra->subtypes_length);
                ir_value_t **offsets = type_type_kind == TYPE_KIND_STRUCTURE ? ir_pool_alloc(pool, sizeof(ir_value_t*) * extra->subtypes_length) : NULL;
                ir_value_t **member_names = ir_pool_alloc(pool, sizeof(ir_value_t*) * extra->subtypes_length);

                ast_elem_t *elem = type_table->entries[i].ast_type.elements[0];

                if(elem->id == AST_ELEM_GENERIC_BASE){
                    ast_elem_generic_base_t *generic_base = (ast_elem_generic_base_t*) elem;
                    
                    // Find polymorphic struct
                    ast_polymorphic_composite_t *template = object_polymorphic_composite_find(NULL, object, &compiler->tmp, generic_base->name, NULL);

                    // TODO: UNIMPLEMENTED: Complex composites are not supported in RTTI yet
                    if(!(ast_layout_is_simple_struct(&template->layout) || ast_layout_is_simple_union(&template->layout))){
                        warningprintf("Runtime type information for complex composite '%s' will not be meaningful (unimplemented)\n", template->name);
                        
                        initializer_members[0] = build_literal_usize(pool, 0); // kind
                        initializer_members[1] = build_literal_cstr_ex(pool, &ir_module->type_map, type_table->entries[i].name); // name
                        initializer_members[2] = build_bool(pool, type_table->entries[i].is_alias); // is_alias
                        initializer_members[3] = build_const_sizeof(pool, rtti_types->usize_type, type_table->entries[i].ir_type); // size

                        if(type_type_kind == TYPE_KIND_STRUCTURE){
                            initializer_members[4] = build_null_pointer_of_type(pool, ir_type_pointer_to(pool, rtti_types->any_type_ptr_type));
                            initializer_members[5] = build_literal_usize(pool, 0); // length
                            initializer_members[6] = build_null_pointer_of_type(pool, ir_type_pointer_to(pool, rtti_types->usize_type));
                            initializer_members[7] = build_null_pointer_of_type(pool, ir_type_pointer_to(pool, rtti_types->ubyte_ptr_type));
                            initializer_members[8] = build_bool(pool, extra->traits & TYPE_KIND_COMPOSITE_PACKED); // is_packed
                            initializer_type = rtti_types->any_struct_type_type;
                        } else {
                            initializer_members[4] = build_null_pointer_of_type(pool, ir_type_pointer_to(pool, rtti_types->any_type_ptr_type));
                            initializer_members[5] = build_literal_usize(pool, 0); // length
                            initializer_members[6] = build_null_pointer_of_type(pool, ir_type_pointer_to(pool, rtti_types->ubyte_ptr_type));
                            initializer_type = rtti_types->any_union_type_type;
                        }

                        ir_value_t *initializer = build_static_struct(ir_module, initializer_type, initializer_members, initializer_members_length, false);
                        build_anon_global_initializer(ir_module, array_values[i], initializer);

                        if(initializer_type != rtti_types->any_type_ptr_type){
                            array_values[i] = build_const_bitcast(pool, array_values[i], rtti_types->any_type_ptr_type);
                        }
                        
                        continue;
                    }
                    
                    if(template == NULL){
                        internalerrorprintf("Failed to find polymorphic composite '%s' that should exist when generating runtime type table!\n", generic_base->name);
                        return NULL;
                    }

                    // Substitute generic type parameters
                    ast_poly_catalog_t catalog;
                    ast_poly_catalog_init(&catalog);

                    if(template->generics_length != generic_base->generics_length){
                        internalerrorprintf("Polymorphic composite '%s' type parameter length mismatch when generating runtime type table!\n", generic_base->name);
                        ast_poly_catalog_free(&catalog);
                        return NULL;
                    }

                    for(length_t i = 0; i != template->generics_length; i++){
                        ast_poly_catalog_add_type(&catalog, template->generics[i], &generic_base->generics[i]);
                    }

                    ast_type_t container_ast_type;
                    if(resolve_type_polymorphics(compiler, type_table, &catalog, &type_table->entries[i].ast_type, &container_ast_type)){
                        ast_poly_catalog_free(&catalog);
                        return NULL;
                    }

                    // Generate meta data
                    for(length_t s = 0; s != extra->subtypes_length; s++){
                        ast_type_t member_ast_type;

                        // DANGEROUS: WARNING: Accessing field based on index going to 'composite->subtypes_length'
                        // DANGEROUS: Assuming non-NULL return value
                        ast_type_t *field_type = ast_layout_skeleton_get_type_at_index(&template->layout.skeleton, s);

                        if(resolve_type_polymorphics(compiler, type_table, &catalog, field_type, &member_ast_type)){
                            ast_poly_catalog_free(&catalog);
                            return NULL;
                        }
                        
                        char *member_type_name = ast_type_str(&member_ast_type);
                        maybe_index_t subtype_index = type_table_find(type_table, member_type_name);
                        free(member_type_name);

                        if(subtype_index == -1){
                            members[s] = build_null_pointer_of_type(pool, rtti_types->any_type_ptr_type); // members[s]
                        } else {
                            members[s] = build_const_bitcast(pool, array_values[subtype_index], rtti_types->any_type_ptr_type); // members[s]
                        }

                        ir_type_t *struct_ir_type;
                        if(ir_gen_resolve_type(compiler, object, &container_ast_type, &struct_ir_type)){
                            internalerrorprintf("ir_gen_resolve_type for RTTI composite offset computation failed!\n");
                            ast_type_free(&container_ast_type);
                            ast_type_free(&member_ast_type);
                            return NULL;
                        }

                        if(offsets) offsets[s] = build_offsetof_ex(pool, rtti_types->usize_type, struct_ir_type, s);
                        member_names[s] = build_literal_cstr_ex(pool, &ir_module->type_map, ast_simple_field_map_get_name_at_index(&template->layout.field_map, s));
                        ast_type_free(&member_ast_type);
                    }

                    ast_type_free(&container_ast_type);
                    ast_poly_catalog_free(&catalog);
                } else if(elem->id == AST_ELEM_BASE){
                    const char *struct_name = ((ast_elem_base_t*) elem)->base;
                    ast_composite_t *composite = object_composite_find(NULL, object, &compiler->tmp, struct_name, NULL);

                    if(composite == NULL){
                        internalerrorprintf("Failed to find composite '%s' that should exist when generating runtime type table!\n", struct_name);
                        return NULL;
                    }

                    // TODO: UNIMPLEMENTED: Complex composites are not supported in RTTI yet
                    if(!(ast_layout_is_simple_struct(&composite->layout) || ast_layout_is_simple_union(&composite->layout))){
                        warningprintf("Runtime type information for complex composite %s will not be meaningful (unimplemented)\n", composite->name);

                        initializer_members[0] = build_literal_usize(pool, 0); // kind
                        initializer_members[1] = build_literal_cstr_ex(pool, &ir_module->type_map, type_table->entries[i].name); // name
                        initializer_members[2] = build_bool(pool, type_table->entries[i].is_alias); // is_alias
                        initializer_members[3] = build_const_sizeof(pool, rtti_types->usize_type, type_table->entries[i].ir_type); // size

                        if(type_type_kind == TYPE_KIND_STRUCTURE){
                            initializer_members[4] = build_null_pointer_of_type(pool, ir_type_pointer_to(pool, rtti_types->any_type_ptr_type));
                            initializer_members[5] = build_literal_usize(pool, 0); // length
                            initializer_members[6] = build_null_pointer_of_type(pool, ir_type_pointer_to(pool, rtti_types->usize_type));
                            initializer_members[7] = build_null_pointer_of_type(pool, ir_type_pointer_to(pool, rtti_types->ubyte_ptr_type));
                            initializer_members[8] = build_bool(pool, extra->traits & TYPE_KIND_COMPOSITE_PACKED); // is_packed
                            initializer_type = rtti_types->any_struct_type_type;
                        } else {
                            initializer_members[4] = build_null_pointer_of_type(pool, ir_type_pointer_to(pool, rtti_types->any_type_ptr_type));
                            initializer_members[5] = build_literal_usize(pool, 0); // length
                            initializer_members[6] = build_null_pointer_of_type(pool, ir_type_pointer_to(pool, rtti_types->ubyte_ptr_type));
                            initializer_type = rtti_types->any_union_type_type;
                        }

                        ir_value_t *initializer = build_static_struct(ir_module, initializer_type, initializer_members, initializer_members_length, false);
                        build_anon_global_initializer(ir_module, array_values[i], initializer);

                        if(initializer_type != rtti_types->any_type_ptr_type){
                            array_values[i] = build_const_bitcast(pool, array_values[i], rtti_types->any_type_ptr_type);
                        }
                        
                        continue;
                    }

                    length_t field_count = ast_simple_field_map_get_count(&composite->layout.field_map);

                    if(field_count != extra->subtypes_length){
                        internalerrorprintf("Mismatching member count of IR as AST types for struct '%s' when generating runtime type table!\n", struct_name);
                        return NULL;
                    }

                    for(length_t s = 0; s != extra->subtypes_length; s++){
                        ast_type_t *field_type = ast_layout_skeleton_get_type_at_index(&composite->layout.skeleton, s);

                        char *member_type_name = ast_type_str(field_type);
                        maybe_index_t subtype_index = type_table_find(type_table, member_type_name);
                        free(member_type_name);

                        if(subtype_index == -1){
                            members[s] = build_null_pointer_of_type(pool, rtti_types->any_type_ptr_type); // members[s]
                        } else {
                            members[s] = build_const_bitcast(pool, array_values[subtype_index], rtti_types->any_type_ptr_type); // members[s]
                        }

                        // SPEED: TODO: Make 'struct_ast_type' not dynamically allocated
                        ast_type_t struct_ast_type;
                        ast_type_make_base(&struct_ast_type, strclone(struct_name));

                        ir_type_t *struct_ir_type;
                        if(ir_gen_resolve_type(compiler, object, &struct_ast_type, &struct_ir_type)){
                            internalerrorprintf("ir_gen_resolve_type for RTTI composite offset computation failed!\n");
                            ast_type_free(&struct_ast_type);
                            return NULL;
                        }

                        ast_type_free(&struct_ast_type);

                        if(offsets) offsets[s] = build_offsetof_ex(pool, rtti_types->usize_type, struct_ir_type, s);
                        member_names[s] = build_literal_cstr_ex(pool, &ir_module->type_map, ast_simple_field_map_get_name_at_index(&composite->layout.field_map, s));
                    }
                } else {
                    internalerrorprintf("Unknown AST type element for TYPE_KIND_STRUCTURE when generating runtime type information!\n");
                    return NULL;
                }

                ir_value_t *members_array = build_static_array(pool, rtti_types->any_type_ptr_type, members, extra->subtypes_length);
                ir_value_t *member_names_array = build_static_array(pool, rtti_types->ubyte_ptr_type, member_names, extra->subtypes_length);
                
                if(type_type_kind == TYPE_KIND_STRUCTURE){
                    ir_value_t *offsets_array = build_static_array(pool, rtti_types->usize_type, offsets, extra->subtypes_length);

                    initializer_members[4] = members_array;
                    initializer_members[5] = build_literal_usize(pool, extra->subtypes_length); // length
                    initializer_members[6] = offsets_array;
                    initializer_members[7] = member_names_array;
                    initializer_members[8] = build_bool(pool, extra->traits & TYPE_KIND_COMPOSITE_PACKED); // is_packed
                    initializer_type = rtti_types->any_struct_type_type;
                } else {
                    initializer_members[4] = members_array;
                    initializer_members[5] = build_literal_usize(pool, extra->subtypes_length); // length
                    initializer_members[6] = member_names_array;
                    initializer_type = rtti_types->any_union_type_type;
                }
            }
            break;
        case TYPE_KIND_FIXED_ARRAY: {
                /* struct AnyFixedArrayType (kind AnyTypeKind, name *ubyte, is_alias bool, size usize, subtype *AnyType, length usize) */

                initializer_members = ir_pool_alloc(pool, sizeof(ir_value_t*) * 6);
                initializer_members_length = 6;

                // DANGEROUS: NOTE: Assumes elements after AST_ELEM_FIXED_ARRAY
                ast_elem_fixed_array_t *elem = (ast_elem_fixed_array_t*) type_table->entries[i].ast_type.elements[0];

                maybe_index_t subtype_index = -1;

                // HACK: We really shouldn't be doing this
                if(type_table->entries[i].ast_type.elements_length > 1){
                    ast_type_t unwrapped = ast_type_clone(&type_table->entries[i].ast_type);

                    ast_type_unwrap_fixed_array(&unwrapped);

                    char *unwrapped_name = ast_type_str(&unwrapped);
                    subtype_index = type_table_find(type_table, unwrapped_name);
                    ast_type_free(&unwrapped);
                    free(unwrapped_name);
                }

                if(subtype_index == -1){
                    ir_value_t *null_pointer = build_null_pointer_of_type(pool, rtti_types->any_type_ptr_type);
                    initializer_members[4] = null_pointer; // subtype
                } else {
                    initializer_members[4] = build_const_bitcast(pool, array_values[subtype_index], rtti_types->any_type_ptr_type); // subtype
                }

                initializer_members[5] = build_literal_usize(pool, elem->length);
                initializer_type = rtti_types->any_fixed_array_type_type;
            }
            break;
        case TYPE_KIND_FUNCPTR: {
                /* struct AnyFuncPtrType (kind AnyTypeKind, name *ubyte, is_alias bool, size usize, args **AnyType, length usize, return_type *AnyType, is_vararg bool, is_stdcall bool) */

                initializer_members = ir_pool_alloc(pool, sizeof(ir_value_t*) * 9);
                initializer_members_length = 9;

                // DANGEROUS: NOTE: Assumes element is AST_ELEM_FUNC
                ast_elem_func_t *elem = (ast_elem_func_t*) type_table->entries[i].ast_type.elements[0];

                maybe_index_t subtype_index = -1;

                // TODO: CLEANUP: Clean up this messy code
                char *type_string;
                ir_value_t **args = ir_pool_alloc(pool, sizeof(ir_value_t*) * elem->arity);

                for(length_t i = 0; i != elem->arity; i++){
                    type_string = ast_type_str(&elem->arg_types[i]);
                    subtype_index = type_table_find(type_table, type_string);
                    free(type_string);

                    if(subtype_index == -1){
                        args[i] = build_null_pointer_of_type(pool, rtti_types->any_type_ptr_type);
                    } else {
                        args[i] = build_const_bitcast(pool, array_values[subtype_index], rtti_types->any_type_ptr_type);
                    }
                }

                type_string = ast_type_str(elem->return_type);
                subtype_index = type_table_find(type_table, type_string);
                free(type_string);
                
                ir_value_t *return_type;
                if(subtype_index == -1){
                    return_type = build_null_pointer_of_type(pool, rtti_types->any_type_ptr_type);
                } else {
                    return_type = build_const_bitcast(pool, array_values[subtype_index], rtti_types->any_type_ptr_type);
                }

                initializer_members[4] = build_static_array(pool, rtti_types->any_type_ptr_type, args, elem->arity);
                initializer_members[5] = build_literal_usize(pool, elem->arity);
                initializer_members[6] = return_type;
                initializer_members[7] = build_bool(pool, elem->traits & AST_FUNC_VARARG);
                initializer_members[8] = build_bool(pool, elem->traits & AST_FUNC_STDCALL);
                initializer_type = rtti_types->any_funcptr_type_type;
            }
            break;
        default:
            initializer_members = ir_pool_alloc(pool, sizeof(ir_value_t*) * 4);
            initializer_members_length = 4;
            initializer_type = rtti_types->any_type_type;
        }

        switch(type_type_kind){
        case TYPE_KIND_NONE:        any_type_kind_id = ANY_TYPE_KIND_VOID; break;
        case TYPE_KIND_POINTER:     any_type_kind_id = ANY_TYPE_KIND_PTR; break;
        case TYPE_KIND_S8:          any_type_kind_id = ANY_TYPE_KIND_BYTE; break;
        case TYPE_KIND_S16:         any_type_kind_id = ANY_TYPE_KIND_SHORT; break;
        case TYPE_KIND_S32:         any_type_kind_id = ANY_TYPE_KIND_INT; break;
        case TYPE_KIND_S64:         any_type_kind_id = ANY_TYPE_KIND_LONG; break;
        case TYPE_KIND_U8:          any_type_kind_id = ANY_TYPE_KIND_UBYTE; break;
        case TYPE_KIND_U16:         any_type_kind_id = ANY_TYPE_KIND_USHORT; break;
        case TYPE_KIND_U32:         any_type_kind_id = ANY_TYPE_KIND_UINT; break;
        case TYPE_KIND_U64:         any_type_kind_id = ANY_TYPE_KIND_ULONG; break;
        case TYPE_KIND_FLOAT:       any_type_kind_id = ANY_TYPE_KIND_FLOAT; break;
        case TYPE_KIND_DOUBLE:      any_type_kind_id = ANY_TYPE_KIND_DOUBLE; break;
        case TYPE_KIND_BOOLEAN:     any_type_kind_id = ANY_TYPE_KIND_BOOL; break;
        case TYPE_KIND_STRUCTURE:   any_type_kind_id = ANY_TYPE_KIND_STRUCT; break;
        case TYPE_KIND_UNION:       any_type_kind_id = ANY_TYPE_KIND_UNION; break;
        case TYPE_KIND_FUNCPTR:     any_type_kind_id = ANY_TYPE_KIND_FUNC_PTR; break;
        case TYPE_KIND_FIXED_ARRAY: any_type_kind_id = ANY_TYPE_KIND_FIXED_ARRAY; break;

        // Unsupported Type Kinds
        case TYPE_KIND_HALF:        any_type_kind_id = ANY_TYPE_KIND_USHORT; break;
        }

        initializer_members[0] = build_literal_usize(pool, any_type_kind_id); // kind
        initializer_members[1] = build_literal_cstr_ex(pool, &ir_module->type_map, type_table->entries[i].name); // name
        initializer_members[2] = build_bool(pool, type_table->entries[i].is_alias); // is_alias
        initializer_members[3] = build_const_sizeof(pool, rtti_types->usize_type, type_table->entries[i].ir_type); // size

        ir_value_t *initializer = build_static_struct(ir_module, initializer_type, initializer_members, initializer_members_length, false);
        build_anon_global_initializer(ir_module, array_values[i], initializer);

        if(initializer_type != rtti_types->any_type_ptr_type){
            array_values[i] = build_const_bitcast(pool, array_values[i], rtti_types->any_type_ptr_type);
        }
    }

    return array_values;
}
