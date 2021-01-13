
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
    || !ir_type_map_find(type_map, "AnyCompositeType", &out_rtti_types->any_composite_type_type)
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

errorcode_t ir_gen__types__pointer_entry(object_t *object, ir_value_t **array_values, length_t array_value_index, ir_rtti_types_t *rtti_types){
    // ---------------------------------------------------------------------------------------------
    // struct AnyPtrType (
    //     kind AnyTypeKind,
    //     name *ubyte,
    //     is_alias bool,
    //     size usize,
    //     subtype *AnyType
    // )
    // ---------------------------------------------------------------------------------------------

    type_table_t *type_table = object->ast.type_table;
    ir_module_t *ir_module = &object->ir_module;
    ir_pool_t *pool = &ir_module->pool;

    ir_value_t **result = &array_values[array_value_index];
    type_table_entry_t *entry = &type_table->entries[array_value_index];

    // Find subtype index
    maybe_index_t subtype_index = -1;
    ir_value_t *subtype_rtti;

    // Find subtype by human-readable name
    if(entry->ast_type.elements_length > 1){
        ast_type_t dereferenced = ast_type_clone(&entry->ast_type);
        ast_type_dereference(&dereferenced);

        char *dereferenced_name = ast_type_str(&dereferenced);
        subtype_index = type_table_find(type_table, dereferenced_name);
        ast_type_free(&dereferenced);
        free(dereferenced_name);
    }

    if(subtype_index == -1){
        subtype_rtti = build_null_pointer_of_type(pool, rtti_types->any_type_ptr_type);
    } else {
        subtype_rtti = build_const_bitcast(pool, array_values[subtype_index], rtti_types->any_type_ptr_type);
    }

    ir_value_t **fields = ir_pool_alloc(pool, sizeof(ir_value_t*) * 5);
    fields[0] = build_literal_usize(pool, ANY_TYPE_KIND_PTR);                     // kind
    fields[1] = build_literal_cstr_ex(pool, &ir_module->type_map, entry->name);   // name
    fields[2] = build_bool(pool, entry->is_alias);                                // is_alias
    fields[3] = build_const_sizeof(pool, rtti_types->usize_type, entry->ir_type); // size
    fields[4] = subtype_rtti;                                                     // subtype

    // Create struct literal and set as initializer
    ir_value_t *initializer = build_static_struct(ir_module, rtti_types->any_ptr_type_type, fields, 5, false);
    build_anon_global_initializer(ir_module, *result, initializer);

    // Bitcast '*AnyPtrType' to '*AnyType'
    *result = build_const_bitcast(pool, *result, rtti_types->any_type_ptr_type);
    return SUCCESS;
}

errorcode_t ir_gen__types__composite_entry(compiler_t *compiler, object_t *object, ir_value_t **array_values, length_t array_value_index, ir_rtti_types_t *rtti_types){
    // ---------------------------------------------------------------------------------------------
    // struct AnyCompositeType (
    //     kind AnyTypeKind,
    //     name *ubyte,
    //     is_alias bool,
    //     size usize,
    //     members **AnyType,
    //     length usize,
    //     offsets *usize,
    //     member_names **ubyte,
    //     is_packed bool
    // )
    // alias AnyStructType = AnyCompositeType
    // alias AnyUnionType  = AnyCompositeType
    // ---------------------------------------------------------------------------------------------

    type_table_t *type_table = object->ast.type_table;
    ir_module_t *ir_module = &object->ir_module;
    ir_pool_t *pool = &ir_module->pool;

    ir_value_t **result = &array_values[array_value_index];
    type_table_entry_t *entry = &type_table->entries[array_value_index];
    ir_type_extra_composite_t *extra_info = (ir_type_extra_composite_t*) entry->ir_type->extra;

    ir_gen_composite_rtti_info_t info;
    if(ir_gen__types__composite_entry_get_info(compiler, object, rtti_types, entry, array_values, &info)){
        return FAILURE;
    }

    ir_value_t *members_array = ir_gen__types__composite_entry_members_array(compiler, object, &info);
    if(members_array == NULL) goto failure;

    ir_value_t *offsets_array = ir_gen__types__composite_entry_offsets_array(object, &info);
    if(offsets_array == NULL) goto failure;

    ir_value_t *member_names_array = ir_gen__types__composite_entry_member_names_array(ir_module, &info);
    if(member_names_array == NULL) goto failure;

    // The number of items in each of the arrays
    ir_value_t *length = build_literal_usize(pool, info.core_composite_info->layout.field_map.arrows_length);

    // Whether or not the struct/union is packed
    ir_value_t *is_packed = build_bool(pool, extra_info->traits & TYPE_KIND_COMPOSITE_PACKED);

    // What kind of composite it is (based on root node)
    length_t kind = entry->ir_type->kind == TYPE_KIND_STRUCTURE ? ANY_TYPE_KIND_STRUCT : ANY_TYPE_KIND_UNION;

    // Dispose of information used to generate internal values
    ir_gen__types__composite_entry_free_info(&info);

    ir_value_t **fields = ir_pool_alloc(pool, sizeof(ir_value_t*) * 9);
    fields[0] = build_literal_usize(pool, kind);                                  // kind
    fields[1] = build_literal_cstr_ex(pool, &ir_module->type_map, entry->name);   // name
    fields[2] = build_bool(pool, entry->is_alias);                                // is_alias
    fields[3] = build_const_sizeof(pool, rtti_types->usize_type, entry->ir_type); // size
    fields[4] = members_array;                                                    // members
    fields[5] = length;                                                           // length
    fields[6] = offsets_array;                                                    // offsets
    fields[7] = member_names_array;                                               // member_names
    fields[8] = is_packed;                                                        // is_packed
    
    // Create struct literal and set as initializer
    ir_value_t *initializer = build_static_struct(ir_module, rtti_types->any_composite_type_type, fields, 9, false);
    build_anon_global_initializer(ir_module, *result, initializer);

    // Bitcast '*AnyCompositeType' to '*AnyType'
    *result = build_const_bitcast(pool, *result, rtti_types->any_type_ptr_type);
    return SUCCESS;

failure:
    ir_gen__types__composite_entry_free_info(&info);
    return FAILURE;
}

errorcode_t ir_gen__types__composite_entry_get_info(compiler_t *compiler, object_t *object, ir_rtti_types_t *rtti_types, type_table_entry_t *entry, ir_value_t **array_values, ir_gen_composite_rtti_info_t *out_info){
    ast_elem_t *first_ast_elem = entry->ast_type.elements[0];

    if(!(first_ast_elem->id == AST_ELEM_BASE || first_ast_elem->id == AST_ELEM_GENERIC_BASE)){
        internalerrorprintf("ir_gen__types__composite_entry_get_info() found 'first_ast_elem' to not be a base-like elem");
        return FAILURE;
    }

    weak_cstr_t name = NULL;
    bool is_polymorphic = first_ast_elem->id == AST_ELEM_GENERIC_BASE;
    ast_composite_t *core_composite_info = NULL;

    // Only exist if 'is_polymorphic'
    ast_poly_catalog_t poly_catalog;
    ast_type_t *maybe_weak_generics = NULL;
    length_t maybe_weak_generics_length = 0;
    
    if(is_polymorphic){
        ast_elem_generic_base_t *generic_base_elem = ((ast_elem_generic_base_t*) first_ast_elem);
        name = generic_base_elem->name;
        maybe_weak_generics = generic_base_elem->generics;
        maybe_weak_generics_length = generic_base_elem->generics_length;

        // Find polymorphic composite
        core_composite_info = (ast_composite_t*) object_polymorphic_composite_find(NULL, object, &compiler->tmp, name, NULL);
    } else {
        name = ((ast_elem_base_t*) first_ast_elem)->base;

        // Find regular composite
        core_composite_info = object_composite_find(NULL, object, &compiler->tmp, name, NULL);
    }

    if(core_composite_info == NULL){
        const char *composite_kind_name = is_polymorphic ? "polymorphic composite" : "composite";
        internalerrorprintf("Failed to find %s '%s' that should exist when generating runtime type table!\n", composite_kind_name, name);
        return FAILURE;
    }

    if(is_polymorphic){
        ast_polymorphic_composite_t *template = (ast_polymorphic_composite_t*) core_composite_info;

        if(template->generics_length != maybe_weak_generics_length){
            internalerrorprintf("Polymorphic composite '%s' type parameter length mismatch when generating runtime type table!\n", name);
            return FAILURE;
        }

        // Initialize polymorphic type catalog
        ast_poly_catalog_init(&poly_catalog);

        // Mention type resolution rules to polymorphic type catalog
        for(length_t i = 0; i != maybe_weak_generics_length; i++){
            ast_poly_catalog_add_type(&poly_catalog, template->generics[i], &maybe_weak_generics[i]);
        }
    }

    out_info->rtti_types = rtti_types;
    out_info->array_values = array_values;
    out_info->core_composite_info = core_composite_info;
    out_info->entry = entry;

    if(is_polymorphic){
        out_info->poly_catalog = poly_catalog;
        out_info->maybe_weak_generics = maybe_weak_generics;
        out_info->maybe_weak_generics_length = maybe_weak_generics_length;
    }
    return SUCCESS;
}

void ir_gen__types__composite_entry_free_info(ir_gen_composite_rtti_info_t *info){
    if(info->core_composite_info->is_polymorphic){
        ast_poly_catalog_free(&info->poly_catalog);
    }
}

ir_value_t *ir_gen__types__composite_entry_members_array(compiler_t *compiler, object_t *object, ir_gen_composite_rtti_info_t *info){
    type_table_t *type_table = object->ast.type_table;
    ir_module_t *ir_module = &object->ir_module;
    ir_pool_t *pool = &ir_module->pool;
    ast_field_map_t *field_map = &info->core_composite_info->layout.field_map;
    ast_layout_skeleton_t *skeleton = &info->core_composite_info->layout.skeleton;

    ir_value_t **members = ir_pool_alloc(pool, sizeof(ir_value_t*) * field_map->arrows_length);

    for(length_t i = 0; i != field_map->arrows_length; i++){
        // WARNING: 'field_type' only has ownership if 'info->core_composite_info->is_polymorphic'
        ast_type_t field_type;

        // Pointer to the field type which could be polymorphic
        ast_type_t *unprocessed_field_type = ast_layout_skeleton_get_type(skeleton, field_map->arrows[i].endpoint);

        // Resolve any polymorphics in field type
        if(info->core_composite_info->is_polymorphic){
            if(resolve_type_polymorphics(compiler, type_table, &info->poly_catalog, unprocessed_field_type, &field_type))
                return NULL;
        } else {
            field_type = *unprocessed_field_type;
        }

        // Get RTTI of subtype by name
        strong_cstr_t subtype_name = ast_type_str(&field_type);
        maybe_index_t subtype_index = type_table_find(type_table, subtype_name);
        free(subtype_name);

        // Fill in RTTI for member subtype index
        if(subtype_index == -1){
            // Use null pointer if subtype wasn't found
            members[i] = build_null_pointer_of_type(pool, info->rtti_types->any_type_ptr_type);
        } else {
            // Use '*AnyType' to RTTI of subtype that was successfully found
            members[i] = build_const_bitcast(pool, info->array_values[subtype_index], info->rtti_types->any_type_ptr_type);
        }

        // WARNING: 'field_type' only has ownership if 'info->core_composite_info->is_polymorphic'
        if(info->core_composite_info->is_polymorphic){
            ast_type_free(&field_type);
        }
    }

    return build_static_array(pool, info->rtti_types->any_type_ptr_type, members, field_map->arrows_length);
}

ir_value_t *ir_gen__types__composite_entry_offsets_array(object_t *object, ir_gen_composite_rtti_info_t *info){
    ir_module_t *ir_module = &object->ir_module;
    ir_pool_t *pool = &ir_module->pool;
    ast_field_map_t *field_map = &info->core_composite_info->layout.field_map;
    ast_layout_t *layout = &info->core_composite_info->layout;

    ir_value_t **offsets = ir_pool_alloc(pool, sizeof(ir_value_t*) * field_map->arrows_length);
    ir_type_t *root_ir_type = info->entry->ir_type;

    for(length_t i = 0; i != field_map->arrows_length; i++){
        // Pointer to the field type which could be polymorphic
        ast_layout_endpoint_t endpoint = field_map->arrows[i].endpoint;

        // Compute endpoint path
        ast_layout_endpoint_path_t path;
        if(!ast_layout_get_path(layout, endpoint, &path)) return NULL;

        // Use endpoint path and skeleton to compute the field's offset
        ir_value_t *offset = ast_layout_path_get_offset(ir_module, &endpoint, &path, root_ir_type);
        if(offset == NULL) return NULL;

        offsets[i] = offset;
    }

    return build_static_array(pool, info->rtti_types->usize_type, offsets, field_map->arrows_length);
}

ir_value_t *ir_gen__types__composite_entry_member_names_array(ir_module_t *ir_module, ir_gen_composite_rtti_info_t *info){
    ir_pool_t *pool = &ir_module->pool;
    ir_type_map_t *type_map = &ir_module->type_map;
    ast_field_map_t *field_map = &info->core_composite_info->layout.field_map;

    ir_value_t **member_names = ir_pool_alloc(pool, sizeof(ir_value_t*) * field_map->arrows_length);

    for(length_t i = 0; i != field_map->arrows_length; i++){
        member_names[i] = build_literal_cstr_ex(pool, type_map, field_map->arrows[i].name);
    }

    return build_static_array(pool, info->rtti_types->ubyte_ptr_type, member_names, field_map->arrows_length);
}

ir_value_t **ir_gen__types__values(compiler_t *compiler, object_t *object, ir_rtti_types_t *rtti_types){
    // Returns NULL on failure

    ir_module_t *ir_module = &object->ir_module;
    ir_pool_t *pool = &ir_module->pool;
    type_table_t *type_table = object->ast.type_table;
    
    ir_value_t **array_values = ir_pool_alloc(pool, sizeof(ir_value_t*) * type_table->length);
    
    // Preparation for each array value
    if(ir_gen__types__prepare_each_value(compiler, object, rtti_types, array_values)) return NULL;

    // TODO: CLEANUP: Cleanup this messy code
    for(length_t i = 0; i != type_table->length; i++){
        ir_type_t *initializer_type;
        ir_value_t **initializer_members;
        length_t initializer_members_length;
        unsigned int any_type_kind_id = ANY_TYPE_KIND_VOID;
        unsigned int type_type_kind = type_table->entries[i].ir_type->kind;

        switch(type_type_kind){
        case TYPE_KIND_POINTER:
            if(ir_gen__types__pointer_entry(object, array_values, i, rtti_types)) return NULL;
            continue;
        case TYPE_KIND_STRUCTURE: case TYPE_KIND_UNION:
            if(ir_gen__types__composite_entry(compiler, object, array_values, i, rtti_types)) return NULL;
            continue;
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

errorcode_t ir_gen__types__prepare_each_value(compiler_t *compiler, object_t *object, ir_rtti_types_t *rtti_types, ir_value_t **array_values){
    // NOTE: 'array_values' must have a length of 'type_table->length'

    ir_module_t *ir_module = &object->ir_module;
    type_table_t *type_table = object->ast.type_table;

    for(length_t i = 0; i != type_table->length; i++){
        // Resolve AST type to IR type for each type table entry
        if(ir_gen_resolve_type(compiler, object, &type_table->entries[i].ast_type, &type_table->entries[i].ir_type)){
            return FAILURE;
        }

        // Create foundation for each array value that is of the correct AnyType variant
        ir_type_t *any_type_variant;

        switch(type_table->entries[i].ir_type->kind){
        case TYPE_KIND_POINTER:
            any_type_variant = rtti_types->any_ptr_type_type;
            break;
        case TYPE_KIND_UNION:
        case TYPE_KIND_STRUCTURE:
            any_type_variant = rtti_types->any_composite_type_type;
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

    return SUCCESS;
}
