
#include "AST/ast.h"
#include "AST/ast_type.h"
#include "UTIL/util.h"
#include "UTIL/color.h"

ast_elem_t *ast_elem_clone(const ast_elem_t *element){
    ast_elem_t *new_element = NULL;

    switch(element->id){
    case AST_ELEM_BASE:
        new_element = malloc(sizeof(ast_elem_base_t));
        ((ast_elem_base_t*) new_element)->id = AST_ELEM_BASE;
        ((ast_elem_base_t*) new_element)->source = element->source;
        ((ast_elem_base_t*) new_element)->base = strclone(((ast_elem_base_t*) element)->base);
        break;
    case AST_ELEM_POINTER:
        new_element = malloc(sizeof(ast_elem_pointer_t));
        ((ast_elem_pointer_t*) new_element)->id = AST_ELEM_POINTER;
        ((ast_elem_pointer_t*) new_element)->source = element->source;
        break;
    case AST_ELEM_ARRAY:
        new_element = malloc(sizeof(ast_elem_array_t));
        ((ast_elem_array_t*) new_element)->id = AST_ELEM_ARRAY;
        ((ast_elem_array_t*) new_element)->source = element->source;
        break;
    case AST_ELEM_FIXED_ARRAY:
        new_element = malloc(sizeof(ast_elem_fixed_array_t));
        ((ast_elem_fixed_array_t*) new_element)->id = AST_ELEM_FIXED_ARRAY;
        ((ast_elem_fixed_array_t*) new_element)->source = element->source;
        ((ast_elem_fixed_array_t*) new_element)->length = ((ast_elem_fixed_array_t*) element)->length;
        break;
    case AST_ELEM_GENERIC_INT:
        new_element = malloc(sizeof(ast_elem_t));
        new_element->id = AST_ELEM_GENERIC_INT;
        new_element->source = element->source;
        break;
    case AST_ELEM_GENERIC_FLOAT:
        new_element = malloc(sizeof(ast_elem_t));
        new_element->id = AST_ELEM_GENERIC_FLOAT;
        new_element->source = element->source;
        break;
    case AST_ELEM_FUNC:
        new_element = malloc(sizeof(ast_elem_func_t));
        ((ast_elem_func_t*) new_element)->id = AST_ELEM_FUNC;
        ((ast_elem_func_t*) new_element)->source = ((ast_elem_func_t*) element)->source;

        ((ast_elem_func_t*) new_element)->arg_types = malloc(sizeof(ast_type_t) * ((ast_elem_func_t*) element)->arity);
        for(length_t i = 0; i != ((ast_elem_func_t*) element)->arity; i++){
            ((ast_elem_func_t*) new_element)->arg_types[i] = ast_type_clone(&((ast_elem_func_t*) element)->arg_types[i]);
        }

        ((ast_elem_func_t*) new_element)->arg_flows = malloc(sizeof(char) * ((ast_elem_func_t*) element)->arity);
        memcpy(((ast_elem_func_t*) new_element)->arg_flows, ((ast_elem_func_t*) element)->arg_flows, sizeof(char) * ((ast_elem_func_t*) element)->arity);
        ((ast_elem_func_t*) new_element)->arity = ((ast_elem_func_t*) element)->arity;
        ((ast_elem_func_t*) new_element)->return_type = malloc(sizeof(ast_type_t));
        *((ast_elem_func_t*) new_element)->return_type = ast_type_clone(((ast_elem_func_t*) element)->return_type);
        ((ast_elem_func_t*) new_element)->traits = ((ast_elem_func_t*) element)->traits;
        ((ast_elem_func_t*) new_element)->ownership = true;
        break;
    case AST_ELEM_POLYMORPH: {
            new_element = malloc(sizeof(ast_elem_polymorph_t));
            ((ast_elem_polymorph_t*) new_element)->id = AST_ELEM_POLYMORPH;
            ((ast_elem_polymorph_t*) new_element)->source = element->source;
            ((ast_elem_polymorph_t*) new_element)->name = strclone(((ast_elem_polymorph_t*) element)->name);
            break;
        }
    case AST_ELEM_GENERIC_BASE: {
            new_element = malloc(sizeof(ast_elem_generic_base_t));
            ((ast_elem_generic_base_t*) new_element)->id = AST_ELEM_GENERIC_BASE;
            ((ast_elem_generic_base_t*) new_element)->source = element->source;
            ((ast_elem_generic_base_t*) new_element)->name = strclone(((ast_elem_generic_base_t*) element)->name);
            ((ast_elem_generic_base_t*) new_element)->generics = malloc(sizeof(ast_type_t) * ((ast_elem_generic_base_t*) element)->generics_length);
            
            for(length_t i = 0; i != ((ast_elem_generic_base_t*) element)->generics_length; i++){
                ((ast_elem_generic_base_t*) new_element)->generics[i] = ast_type_clone(&((ast_elem_generic_base_t*) element)->generics[i]);
            }

            ((ast_elem_generic_base_t*) new_element)->generics_length = ((ast_elem_generic_base_t*) element)->generics_length;
            ((ast_elem_generic_base_t*) new_element)->name_is_polymorphic = ((ast_elem_generic_base_t*) element)->name_is_polymorphic;
            break;
        }
    default:
        redprintf("INTERNAL ERROR: Encountered unexpected type element id when cloning ast_elem_t, a crash will probably follow...\n");
    }

    return new_element;
}

ast_type_t ast_type_clone(const ast_type_t *original){
    ast_type_t new;
    new.elements = malloc(sizeof(ast_elem_t*) * original->elements_length);
    new.elements_length = original->elements_length;
    new.source = original->source;

    for(length_t i = 0; i != original->elements_length; i++){
        new.elements[i] = ast_elem_clone(original->elements[i]);
    }

    return new;
}

void ast_type_free(ast_type_t *type){
    for(length_t i = 0; i != type->elements_length; i++){
        switch(type->elements[i]->id){
        case AST_ELEM_BASE:
            free(((ast_elem_base_t*) type->elements[i])->base);
            free(type->elements[i]);
            break;
        case AST_ELEM_POINTER:
        case AST_ELEM_ARRAY:
        case AST_ELEM_FIXED_ARRAY:
        case AST_ELEM_GENERIC_INT:
        case AST_ELEM_GENERIC_FLOAT:
            free(type->elements[i]);
            break;
        case AST_ELEM_FUNC:
            if(((ast_elem_func_t*) type->elements[i])->ownership){
                ast_elem_func_t *func_elem = (ast_elem_func_t*) type->elements[i];
                ast_types_free(func_elem->arg_types, func_elem->arity);
                free(func_elem->arg_types);
                free(func_elem->arg_flows);
                ast_type_free_fully(func_elem->return_type);
                free(type->elements[i]);
            }
            break;
        case AST_ELEM_POLYMORPH:
            free(((ast_elem_polymorph_t*) type->elements[i])->name);
            free(type->elements[i]);
            break;
        case AST_ELEM_GENERIC_BASE: {
                ast_elem_generic_base_t *generic_base_elem = (ast_elem_generic_base_t*) type->elements[i];
                ast_types_free_fully(generic_base_elem->generics, generic_base_elem->generics_length);
                free(generic_base_elem->name);
                free(type->elements[i]);
            }
            break;
        default:
            redprintf("INTERNAL ERROR: Encountered unexpected type element id when freeing ast_type_t\n");
        }
    }

    free(type->elements);
}

void ast_type_free_fully(ast_type_t *type){
    ast_type_free(type);
    free(type);
}

void ast_types_free(ast_type_t *types, length_t length){
    for(length_t t = 0; t != length; t++) ast_type_free(&types[t]);
}

void ast_types_free_fully(ast_type_t *types, length_t length){
    for(length_t t = 0; t != length; t++) ast_type_free(&types[t]);
    free(types);
}

void ast_type_make_base(ast_type_t *type, strong_cstr_t base){
    ast_elem_base_t *elem = malloc(sizeof(ast_elem_base_t));
    elem->id = AST_ELEM_BASE;
    elem->base = base;
    elem->source = NULL_SOURCE;

    type->elements = malloc(sizeof(ast_elem_t*));
    type->elements[0] = (ast_elem_t*) elem;
    type->elements_length = 1;
    type->source = NULL_SOURCE;
}

void ast_type_make_base_ptr(ast_type_t *type, strong_cstr_t base){
    ast_elem_base_t *elem = malloc(sizeof(ast_elem_base_t));
    ast_elem_pointer_t *ptr = malloc(sizeof(ast_elem_pointer_t));
    elem->id = AST_ELEM_BASE;
    elem->base = base;
    elem->source = NULL_SOURCE;

    ptr->id = AST_ELEM_POINTER;
    ptr->source = NULL_SOURCE;

    type->elements = malloc(sizeof(ast_elem_t*) * 2);
    type->elements[0] = (ast_elem_t*) ptr;
    type->elements[1] = (ast_elem_t*) elem;
    type->elements_length = 2;
    type->source = NULL_SOURCE;
}

void ast_type_make_base_ptr_ptr(ast_type_t *type, strong_cstr_t base){
    ast_elem_base_t *elem = malloc(sizeof(ast_elem_base_t));
    ast_elem_pointer_t *ptr1 = malloc(sizeof(ast_elem_pointer_t));
    ast_elem_pointer_t *ptr2 = malloc(sizeof(ast_elem_pointer_t));
    elem->id = AST_ELEM_BASE;
    elem->base = base;
    elem->source = NULL_SOURCE;

    ptr1->id = AST_ELEM_POINTER;
    ptr1->source = NULL_SOURCE;

    ptr2->id = AST_ELEM_POINTER;
    ptr2->source = NULL_SOURCE;

    type->elements = malloc(sizeof(ast_elem_t*) * 3);
    type->elements[0] = (ast_elem_t*) ptr1;
    type->elements[1] = (ast_elem_t*) ptr2;
    type->elements[2] = (ast_elem_t*) elem;
    type->elements_length = 3;
    type->source = NULL_SOURCE;
}

void ast_type_prepend_ptr(ast_type_t *type){
    // Prepends a '*' to an existing ast_type_t

    ast_elem_t **new_elements = malloc(sizeof(ast_elem_t*) * (type->elements_length + 1));
    memcpy(&new_elements[1], type->elements, sizeof(ast_elem_t*) * type->elements_length);
    free(type->elements);

    ast_elem_pointer_t *ptr = malloc(sizeof(ast_elem_pointer_t));
    ptr->id = AST_ELEM_POINTER;
    ptr->source = NULL_SOURCE;
    new_elements[0] = (ast_elem_t*) ptr;

    type->elements = new_elements;
    type->elements_length++;
}

void ast_type_make_polymorph(ast_type_t *type, strong_cstr_t name){
    ast_elem_polymorph_t *elem = malloc(sizeof(ast_elem_polymorph_t));
    elem->id = AST_ELEM_POLYMORPH;
    elem->name = name;
    elem->source = NULL_SOURCE;

    type->elements = malloc(sizeof(ast_elem_t*));
    type->elements[0] = (ast_elem_t*) elem;
    type->elements_length = 1;
    type->source = NULL_SOURCE;
}

void ast_type_make_generic_int(ast_type_t *type){
    ast_elem_base_t *elem = malloc(sizeof(ast_elem_t));
    elem->id = AST_ELEM_GENERIC_INT;
    elem->source = NULL_SOURCE;

    type->elements = malloc(sizeof(ast_elem_t*));
    type->elements[0] = (ast_elem_t*) elem;
    type->elements_length = 1;
    type->source = NULL_SOURCE;
}

void ast_type_make_generic_float(ast_type_t *type){
    ast_elem_base_t *elem = malloc(sizeof(ast_elem_t));
    elem->id = AST_ELEM_GENERIC_FLOAT;
    elem->source = NULL_SOURCE;

    type->elements = malloc(sizeof(ast_elem_t*));
    type->elements[0] = (ast_elem_t*) elem;
    type->elements_length = 1;
    type->source = NULL_SOURCE;
}

strong_cstr_t ast_type_str(const ast_type_t *type){
    // NOTE: Returns allocated string containing string representation of type
    // NOTE: Returns NULL on error

    char *name = malloc(256);
    length_t name_length = 0;
    length_t name_capacity = 256;

    name[0] = '\0'; // Probably unnecessary but good for safety

    #define EXTEND_NAME_MACRO(extend_name_amount) { \
        if(name_length + extend_name_amount >= name_capacity){ \
            char *new_name = malloc(name_capacity * 2); \
            memcpy(new_name, name, name_length + 1); \
            free(name); \
            name = new_name; \
            name_capacity *= 2; \
        } \
    }

    for(length_t i = 0; i != type->elements_length; i++){
        switch(type->elements[i]->id){
        case AST_ELEM_BASE: {
                const char *base = ((ast_elem_base_t*) type->elements[i])->base;
                length_t base_length = strlen(base);
                EXTEND_NAME_MACRO(base_length);
                memcpy(&name[name_length], base, base_length + 1);
                name_length += base_length;
            }
            break;
        case AST_ELEM_POINTER:
            EXTEND_NAME_MACRO(1);
            memcpy(&name[name_length], "*", 2);
            name_length += 1;
            break;
        case AST_ELEM_ARRAY:
            break;
        case AST_ELEM_FIXED_ARRAY: {
                char fixed_array_length_buffer[32];
                length_t fixed_array_length = ((ast_elem_fixed_array_t*) type->elements[i])->length;
                sprintf(fixed_array_length_buffer, "%d ", (int) fixed_array_length);
                length_t fixed_array_length_buffer_length = strlen(fixed_array_length_buffer);
                EXTEND_NAME_MACRO(fixed_array_length_buffer_length);
                memcpy(&name[name_length], fixed_array_length_buffer, fixed_array_length_buffer_length + 1);
                name_length += fixed_array_length_buffer_length;
            }
            break;
        case AST_ELEM_GENERIC_INT:
            EXTEND_NAME_MACRO(3);
            memcpy(&name[name_length], "int", 4); // Collapse to 'int'
            name_length += 3;
            break;
        case AST_ELEM_GENERIC_FLOAT:
            EXTEND_NAME_MACRO(6);
            memcpy(&name[name_length], "double", 7); // Collapse to 'double'
            name_length += 6;
            break;
        case AST_ELEM_FUNC: {
                ast_elem_func_t *func_elem = (ast_elem_func_t*) type->elements[i];
                length_t type_str_length;
                char *type_str;

                if(func_elem->traits & AST_FUNC_STDCALL){
                    EXTEND_NAME_MACRO(8);
                    memcpy(&name[name_length], "stdcall ", 8);
                    name_length += 8;
                }

                EXTEND_NAME_MACRO(5);
                memcpy(&name[name_length], "func(", 5);
                name_length += 5;

                // do args
                for(length_t a = 0; a != func_elem->arity; a++){
                    type_str = ast_type_str(&func_elem->arg_types[a]);
                    type_str_length = strlen(type_str);

                    EXTEND_NAME_MACRO(type_str_length);
                    memcpy(&name[name_length], type_str, type_str_length);
                    name_length += type_str_length;
                    free(type_str);

                    if(a != func_elem->arity - 1){
                        memcpy(&name[name_length], ", ", 3);
                        name_length += 2;
                    } else if(func_elem->traits & AST_FUNC_VARARG){
                        EXTEND_NAME_MACRO(5);
                        memcpy(&name[name_length], ", ...", 5);
                        name_length += 5;
                    }
                }

                EXTEND_NAME_MACRO(3);
                memcpy(&name[name_length], ") ", 3);
                name_length += 2;

                type_str = ast_type_str(func_elem->return_type);
                type_str_length = strlen(type_str);

                EXTEND_NAME_MACRO(type_str_length);
                memcpy(&name[name_length], type_str, type_str_length + 1);
                name_length += type_str_length;
                free(type_str);
            }
            break;
        case AST_ELEM_POLYMORPH: {
                const char *polyname = ((ast_elem_polymorph_t*) type->elements[i])->name;
                length_t polyname_length = strlen(polyname);
                EXTEND_NAME_MACRO(polyname_length + 1);
                name[name_length] = '$';
                memcpy(&name[name_length + 1], polyname, polyname_length + 1);
                name_length += polyname_length + 1;
            }
            break;
        case AST_ELEM_GENERIC_BASE: {
                ast_elem_generic_base_t *generic_base = (ast_elem_generic_base_t*) type->elements[i];

                EXTEND_NAME_MACRO(1);
                memcpy(&name[name_length], "<", 1);
                name_length += 1;

                for(length_t i = 0; i != generic_base->generics_length; i++){
                    strong_cstr_t type_str = ast_type_str(&generic_base->generics[i]);
                    length_t type_str_length = strlen(type_str);

                    EXTEND_NAME_MACRO(type_str_length);
                    memcpy(&name[name_length], type_str, type_str_length);
                    name_length += type_str_length;
                    free(type_str);

                    if(i != generic_base->generics_length - 1){
                        memcpy(&name[name_length], ", ", 2);
                        name_length += 2;
                    }
                }

                EXTEND_NAME_MACRO(2);
                memcpy(&name[name_length], "> ", 2);
                name_length += 2;

                if(generic_base->name_is_polymorphic){
                    EXTEND_NAME_MACRO(1);
                    memcpy(&name[name_length], "$", 1);
                    name_length += 1;
                }

                length_t base_length = strlen(generic_base->name);
                EXTEND_NAME_MACRO(base_length + 1);
                memcpy(&name[name_length], generic_base->name, base_length + 1);
                name_length += base_length;
            }
            break;
        default:
            printf("INTERNAL ERROR: Encountered unexpected element type 0x%08X when converting ast_type_t to a string\n", type->elements[i]->id);
            return NULL;
        }
    }

    #undef EXTEND_NAME_MACRO
    return name;
}

bool ast_types_identical(const ast_type_t *a, const ast_type_t *b){
    // NOTE: Returns true if the two types are identical
    // NOTE: The two types must be exactly the same to be considered identical (Exception is 'ulong' and 'usize')
    // NOTE: Even if two types represent the same type, they are still not identical
    // [pointer] [base "ubyte"]    [base "CStringAlias"]      -> false
    // [pointer] [base "ubyte"]    [pointer] [base: "ubyte"]  -> true

    if(a->elements_length != b->elements_length) return false;

    for(length_t i = 0; i != a->elements_length; i++){
        unsigned int id = a->elements[i]->id;

        if(id != b->elements[i]->id) return false;

        // Do more specific checking if needed
        switch(id){
        case AST_ELEM_BASE:
            if(strcmp(((ast_elem_base_t*) a->elements[i])->base, "usize") == 0 || strcmp(((ast_elem_base_t*) a->elements[i])->base, "ulong") == 0){
                // Cheap little hack to get 'usize' to be treated the same as 'ulong'
                // TODO: Create special function func non-identical comparison, or modify the name of this function
                if(strcmp(((ast_elem_base_t*) b->elements[i])->base, "usize") == 0 || strcmp(((ast_elem_base_t*) b->elements[i])->base, "ulong") == 0)
                    break;
            }
            if(strcmp(((ast_elem_base_t*) a->elements[i])->base, "successful") == 0 || strcmp(((ast_elem_base_t*) a->elements[i])->base, "bool") == 0){
                // Cheap little hack to get 'successful' to be treated the same as 'bool'
                // TODO: Create special function func non-identical comparison, or modify the name of this function
                if(strcmp(((ast_elem_base_t*) b->elements[i])->base, "successful") == 0 || strcmp(((ast_elem_base_t*) b->elements[i])->base, "bool") == 0)
                    break;
            }
            if(strcmp(((ast_elem_base_t*) a->elements[i])->base, ((ast_elem_base_t*) b->elements[i])->base) != 0) return false;
            break;
        case AST_ELEM_FIXED_ARRAY:
            if(((ast_elem_fixed_array_t*) a->elements[i])->length != ((ast_elem_fixed_array_t*) b->elements[i])->length) return false;
            break;
        case AST_ELEM_FUNC: {
                ast_elem_func_t *func_elem_a = (ast_elem_func_t*) a->elements[i];
                ast_elem_func_t *func_elem_b = (ast_elem_func_t*) b->elements[i];
                if((func_elem_a->traits & AST_FUNC_VARARG) != (func_elem_b->traits & AST_FUNC_VARARG)) return false;
                if((func_elem_a->traits & AST_FUNC_STDCALL) != (func_elem_b->traits & AST_FUNC_STDCALL)) return false;
                if(func_elem_a->arity != func_elem_b->arity) return false;
                if(!ast_types_identical(func_elem_a->return_type, func_elem_b->return_type)) return false;

                for(length_t a = 0; a != func_elem_a->arity; a++){
                    if(!ast_types_identical(&func_elem_a->arg_types[a], &func_elem_b->arg_types[a])) return false;
                    if(func_elem_a->arg_flows[a] != func_elem_b->arg_flows[a]) return false;
                }
            }
            break;
        }
    }

    return true;
}

bool ast_type_is_void(const ast_type_t *type){
    if(type->elements_length != 1 || type->elements[0]->id != AST_ELEM_BASE) return false;
    if(strcmp(((ast_elem_base_t*) type->elements[0])->base, "void") != 0) return false;
    return true;
}

bool ast_type_is_base(const ast_type_t *type){
    if(type->elements_length != 1) return false;
    if(type->elements[0]->id != AST_ELEM_BASE) return false;
    return true;
}

bool ast_type_is_base_ptr(const ast_type_t *type){
    if(type->elements_length != 2) return false;
    if(type->elements[0]->id != AST_ELEM_POINTER || type->elements[1]->id != AST_ELEM_BASE) return false;
    return true;
}

bool ast_type_is_base_of(const ast_type_t *type, const char *base){
    if(type->elements_length != 1) return false;
    if(type->elements[0]->id != AST_ELEM_BASE) return false;
    if(strcmp(((ast_elem_base_t*) type->elements[0])->base, base) != 0) return false;
    return true;
}

bool ast_type_is_base_ptr_of(const ast_type_t *type, const char *base){
    if(type->elements_length != 2) return false;
    if(type->elements[0]->id != AST_ELEM_POINTER || type->elements[1]->id != AST_ELEM_BASE) return false;
    if(strcmp(((ast_elem_base_t*) type->elements[1])->base, base) != 0) return false;
    return true;
}

bool ast_type_is_pointer_to(const ast_type_t *type, const ast_type_t *to){
    if(type->elements_length < 2 || type->elements_length != to->elements_length + 1) return false;
    if(type->elements[0]->id != AST_ELEM_POINTER) return false;

    ast_type_t stripped = *type;
    ast_elem_t *stripped_elements[to->elements_length];
    memcpy(stripped_elements, &stripped.elements[1], sizeof(ast_elem_t*) * to->elements_length);
    stripped.elements = stripped_elements;
    stripped.elements_length--;
    
    return ast_types_identical(&stripped, to);
}

bool ast_type_has_polymorph(const ast_type_t *type){
    for(length_t i = 0; i != type->elements_length; i++){
        switch(type->elements[i]->id){
        case AST_ELEM_BASE:
        case AST_ELEM_POINTER:
        case AST_ELEM_GENERIC_INT:
        case AST_ELEM_GENERIC_FLOAT:
        case AST_ELEM_FIXED_ARRAY:
            break;
        case AST_ELEM_FUNC: {
                ast_elem_func_t *func_elem = (ast_elem_func_t*) type->elements[i];
                for(length_t i = 0; i != func_elem->arity; i++){
                    if(ast_type_has_polymorph(&func_elem->arg_types[i])) return true;
                }
                if(ast_type_has_polymorph(func_elem->return_type)) return true;
            }
            break;
        case AST_ELEM_POLYMORPH:
            return true;
        case AST_ELEM_GENERIC_BASE: {
                ast_elem_generic_base_t *generic_base = (ast_elem_generic_base_t*) type->elements[i];
                if(generic_base->name_is_polymorphic) return true;
                
                for(length_t i = 0; i != generic_base->generics_length; i++){
                    if(ast_type_has_polymorph(&generic_base->generics[i])) return true;
                }
            }
            break;
        default:
            redprintf("INTERNAL ERROR: ast_type_has_polymorph encountered unknown element id 0x%8X\n", type->elements[i]->id);
        }
    }
    return false;
}

bool ast_type_polymorphable(const ast_type_t *polymorphic_type, const ast_type_t *concrete_type, ast_type_var_catalog_t *catalog){
    if(polymorphic_type->elements_length > concrete_type->elements_length) return false;

    for(length_t i = 0; i != concrete_type->elements_length; i++){
        length_t poly_elem_id = polymorphic_type->elements[i]->id;

        // Check whether element is a polymorphic element
        if(poly_elem_id == AST_ELEM_POLYMORPH){
            // Ensure there aren't other elements following the polymorphic element
            if(i + 1 != polymorphic_type->elements_length){
                redprintf("INTERNAL ERROR: ast_type_polymorphable encountered polymorphic element in middle of AST type\n");
                return false;
            }

            // DANGEROUS: Manually managed AST type with semi-ownership
            // DANGEROUS: Potentially bad memory tricks
            ast_type_t replacement;
            replacement.elements_length = concrete_type->elements_length - i;
            replacement.elements = malloc(sizeof(ast_elem_t*) * replacement.elements_length);
            memcpy(replacement.elements, &concrete_type->elements[i], sizeof(ast_elem_t*) * replacement.elements_length);
            replacement.source = replacement.elements[0]->source;

            // Ensure consistency with catalog
            ast_elem_polymorph_t *polymorphic_elem = (ast_elem_polymorph_t*) polymorphic_type->elements[i];
            ast_type_var_t *type_var = ast_type_var_catalog_find(catalog, polymorphic_elem->name);

            if(type_var){
                if(!ast_types_identical(&replacement, &type_var->binding)){
                    // Given arguments don't meet consistency requirements of type variables
                    free(replacement.elements);
                    return false;
                }
            } else {
                // Add to catalog since it's not already present
                ast_type_var_catalog_add(catalog, polymorphic_elem->name, &replacement);
            }

            i = concrete_type->elements_length - 1;
            free(replacement.elements);
            continue;
        }

        // If the polymorphic type doesn't have a polymorphic element for the current element,
        // then the type element ids must match
        if(poly_elem_id != concrete_type->elements[i]->id) return false;
        
        // Ensure the two non-polymorphic elements match
        switch(poly_elem_id){
        case AST_ELEM_BASE:
            if(strcmp(((ast_elem_base_t*) polymorphic_type->elements[i])->base, ((ast_elem_base_t*) concrete_type->elements[i])->base) != 0)
                return false;
            break;
        case AST_ELEM_POINTER:
        case AST_ELEM_GENERIC_INT:
        case AST_ELEM_GENERIC_FLOAT:
            // Always considered matching
            break;
        case AST_ELEM_FIXED_ARRAY:
            if(((ast_elem_fixed_array_t*) polymorphic_type->elements[i])->length != ((ast_elem_fixed_array_t*) concrete_type->elements[i])->length) return false;
            break;
        case AST_ELEM_FUNC: {
                ast_elem_func_t *func_elem_a = (ast_elem_func_t*) polymorphic_type->elements[i];
                ast_elem_func_t *func_elem_b = (ast_elem_func_t*) concrete_type->elements[i];
                if((func_elem_a->traits & AST_FUNC_VARARG) != (func_elem_b->traits & AST_FUNC_VARARG)) return false;
                if((func_elem_a->traits & AST_FUNC_STDCALL) != (func_elem_b->traits & AST_FUNC_STDCALL)) return false;
                if(func_elem_a->arity != func_elem_b->arity) return false;
                if(!ast_type_polymorphable(func_elem_a->return_type, func_elem_b->return_type, catalog)) return false;

                for(length_t a = 0; a != func_elem_a->arity; a++){
                    if(!ast_type_polymorphable(&func_elem_a->arg_types[a], &func_elem_b->arg_types[a], catalog)) return false;
                    if(func_elem_a->arg_flows[a] != func_elem_b->arg_flows[a]) return false;
                }
            }
            break;
        case AST_ELEM_POLYMORPH:
            redprintf("INTERNAL ERROR: ast_type_polymorphable encountered uncaught polymorphic type element\n");
            return false;
        default:
            redprintf("INTERNAL ERROR: ast_type_polymorphable encountered unknown element id 0x%8X\n", polymorphic_type->elements[i]->id);
            return false;
        }
    }

    return true;
}

void ast_type_var_catalog_init(ast_type_var_catalog_t *catalog){
    catalog->type_vars = NULL;
    catalog->length = 0;
    catalog->capacity = 0;
}

void ast_type_var_catalog_free(ast_type_var_catalog_t *catalog){
    for(length_t i = 0; i != catalog->length; i++){
        ast_type_free(&catalog->type_vars[i].binding);
    }
    free(catalog->type_vars);
}

void ast_type_var_catalog_add(ast_type_var_catalog_t *catalog, weak_cstr_t name, ast_type_t *binding){
    expand((void**) &catalog->type_vars, sizeof(ast_type_var_t), catalog->length, &catalog->capacity, 1, 4);
    ast_type_var_t *type_var = &catalog->type_vars[catalog->length++];
    type_var->name = name;
    type_var->binding = ast_type_clone(binding);
}

ast_type_var_t *ast_type_var_catalog_find(ast_type_var_catalog_t *catalog, weak_cstr_t name){
    // TODO: SPEED: Improve searching (maybe not worth it?)

    for(length_t i = 0; i != catalog->length; i++){
        if(strcmp(catalog->type_vars[i].name, name) == 0) return &catalog->type_vars[i];
    }

    return NULL;
}
