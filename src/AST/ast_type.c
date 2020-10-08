
#include "AST/ast.h"
#include "AST/ast_type.h"
#include "UTIL/util.h"
#include "UTIL/hash.h"
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
    case AST_ELEM_POLYCOUNT:
        new_element = malloc(sizeof(ast_elem_polycount_t));
        ((ast_elem_polycount_t*) new_element)->id = AST_ELEM_POLYCOUNT;
        ((ast_elem_polycount_t*) new_element)->source = element->source;
        ((ast_elem_polycount_t*) new_element)->name = strclone(((ast_elem_polycount_t*) element)->name);
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
            ((ast_elem_polymorph_t*) new_element)->allow_auto_conversion = ((ast_elem_polymorph_t*) element)->allow_auto_conversion;
            break;
        }
    case AST_ELEM_POLYMORPH_PREREQ: {
            new_element = malloc(sizeof(ast_elem_polymorph_prereq_t));
            ((ast_elem_polymorph_prereq_t*) new_element)->id = AST_ELEM_POLYMORPH_PREREQ;
            ((ast_elem_polymorph_prereq_t*) new_element)->source = element->source;
            ((ast_elem_polymorph_prereq_t*) new_element)->name = strclone(((ast_elem_polymorph_prereq_t*) element)->name);
            ((ast_elem_polymorph_prereq_t*) new_element)->similarity_prerequisite = strclone(((ast_elem_polymorph_prereq_t*) element)->similarity_prerequisite);
            ((ast_elem_polymorph_prereq_t*) new_element)->allow_auto_conversion = ((ast_elem_polymorph_prereq_t*) element)->allow_auto_conversion;
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
        internalerrorprintf("Encountered unexpected type element id when cloning ast_elem_t, a crash will probably follow...\n");
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
                ast_type_free_fully(func_elem->return_type);
            }
            free(type->elements[i]);
            break;
        case AST_ELEM_POLYCOUNT:
            free(((ast_elem_polymorph_t*) type->elements[i])->name);
            free(type->elements[i]);
            break;
        case AST_ELEM_POLYMORPH:
            free(((ast_elem_polymorph_t*) type->elements[i])->name);
            free(type->elements[i]);
            break;
        case AST_ELEM_POLYMORPH_PREREQ:
            free(((ast_elem_polymorph_prereq_t*) type->elements[i])->name);
            free(((ast_elem_polymorph_prereq_t*) type->elements[i])->similarity_prerequisite);
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
            internalerrorprintf("Encountered unexpected type element id when freeing ast_type_t\n");
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

void ast_type_make_polymorph(ast_type_t *type, strong_cstr_t name, bool allow_auto_conversion){
    ast_elem_polymorph_t *elem = malloc(sizeof(ast_elem_polymorph_t));
    elem->id = AST_ELEM_POLYMORPH;
    elem->name = name;
    elem->source = NULL_SOURCE;
    elem->allow_auto_conversion = allow_auto_conversion;

    type->elements = malloc(sizeof(ast_elem_t*));
    type->elements[0] = (ast_elem_t*) elem;
    type->elements_length = 1;
    type->source = NULL_SOURCE;
}

void ast_type_make_func_ptr(ast_type_t *type, source_t source, ast_type_t *arg_types, length_t arity, ast_type_t *return_type, trait_t traits, bool have_ownership){
    type->elements = malloc(sizeof(ast_elem_t*));
    type->elements_length = 1;
    type->source = source;

    ast_elem_func_t *elem = malloc(sizeof(ast_elem_func_t));
    elem->id = AST_ELEM_FUNC;
    elem->source = source;
    elem->arg_types = arg_types;
    elem->arity = arity;
    elem->return_type = return_type;
    elem->traits = traits;
    elem->ownership = have_ownership;

    type->elements[0] = (ast_elem_t*) elem;
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
                    } else if(func_elem->traits & AST_FUNC_VARIADIC){
                        EXTEND_NAME_MACRO(4);
                        memcpy(&name[name_length], ", ..", 4);
                        name_length += 4;
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
                name[name_length++] = '$';

                if(((ast_elem_polymorph_t*) type->elements[i])->allow_auto_conversion){
                    name[name_length++] = '~';
                }

                memcpy(&name[name_length], polyname, polyname_length + 1);
                name_length += polyname_length;
            }
            break;
        case AST_ELEM_POLYMORPH_PREREQ: {
                const char *polyname = ((ast_elem_polymorph_prereq_t*) type->elements[i])->name;
                const char *prereqname = ((ast_elem_polymorph_prereq_t*) type->elements[i])->similarity_prerequisite;

                length_t polyname_length = strlen(polyname);
                length_t prereqname_length = strlen(prereqname);

                EXTEND_NAME_MACRO(polyname_length + prereqname_length + 2);

                name[name_length] = '$';
                memcpy(&name[name_length + 1], polyname, polyname_length);
                name[name_length + 1 + polyname_length] = '~';
                memcpy(&name[name_length + 2 + polyname_length], prereqname, prereqname_length + 1);
                name_length += polyname_length + prereqname_length + 2;
            }
            break;
        case AST_ELEM_POLYCOUNT: {
                const char *polyname = ((ast_elem_polycount_t*) type->elements[i])->name;
                length_t polyname_length = strlen(polyname);
                EXTEND_NAME_MACRO(polyname_length + 3);
                name[name_length++] = '$';
                name[name_length++] = '#';

                memcpy(&name[name_length], polyname, polyname_length + 1);
                name_length += polyname_length;
                name[name_length++] = ' ';
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
                // MAYBE TODO: Create special function func non-identical comparison, or modify the name of this function
                if(strcmp(((ast_elem_base_t*) b->elements[i])->base, "usize") == 0 || strcmp(((ast_elem_base_t*) b->elements[i])->base, "ulong") == 0)
                    break;
            }
            if(strcmp(((ast_elem_base_t*) a->elements[i])->base, "successful") == 0 || strcmp(((ast_elem_base_t*) a->elements[i])->base, "bool") == 0){
                // Cheap little hack to get 'successful' to be treated the same as 'bool'
                // MAYBE TODO: Create special function func non-identical comparison, or modify the name of this function
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
                }
            }
            break;
        case AST_ELEM_POINTER:
            break;
        case AST_ELEM_GENERIC_BASE: {
                ast_elem_generic_base_t *generic_base_a = (ast_elem_generic_base_t*) a->elements[i];
                ast_elem_generic_base_t *generic_base_b = (ast_elem_generic_base_t*) b->elements[i];

                if(generic_base_a->name_is_polymorphic || generic_base_b->name_is_polymorphic){
                    internalerrorprintf("polymorphic names for generic structs not implemented in ast_types_identical\n");
                    return false;
                }

                if(generic_base_a->generics_length != generic_base_b->generics_length) return false;
                if(strcmp(generic_base_a->name, generic_base_b->name) != 0) return false;

                for(length_t i = 0; i != generic_base_a->generics_length; i++){
                    if(!ast_types_identical(&generic_base_a->generics[i], &generic_base_b->generics[i])) return false;
                }
            }
            break;
        case AST_ELEM_POLYMORPH: {
                ast_elem_polymorph_t *polymorph_a = (ast_elem_polymorph_t*) a->elements[i];
                ast_elem_polymorph_t *polymorph_b = (ast_elem_polymorph_t*) b->elements[i];
                if(strcmp(polymorph_a->name, polymorph_b->name) != 0) return false;
                if(polymorph_a->allow_auto_conversion != polymorph_b->allow_auto_conversion) return false;
            }
            break;
        case AST_ELEM_POLYCOUNT: {
                ast_elem_polycount_t *polycount_a = (ast_elem_polycount_t*) a->elements[i];
                ast_elem_polycount_t *polycount_b = (ast_elem_polycount_t*) b->elements[i];
                if(strcmp(polycount_a->name, polycount_b->name) != 0) return false;
            }
            break;
        case AST_ELEM_POLYMORPH_PREREQ: {
                ast_elem_polymorph_prereq_t *polymorph_a = (ast_elem_polymorph_prereq_t*) a->elements[i];
                ast_elem_polymorph_prereq_t *polymorph_b = (ast_elem_polymorph_prereq_t*) b->elements[i];
                if(strcmp(polymorph_a->name, polymorph_b->name) != 0) return false;
                if(strcmp(polymorph_a->similarity_prerequisite, polymorph_b->similarity_prerequisite) != 0) return false;
                if(polymorph_a->allow_auto_conversion != polymorph_b->allow_auto_conversion) return false;
            }
            break;
        default:
            internalerrorprintf("ast_types_identical received unknown element id\n");
            return false;
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

bool ast_type_is_base_like(const ast_type_t *type){
    if(type->elements_length != 1) return false;

    unsigned int elem_id = type->elements[0]->id;

    if(elem_id == AST_ELEM_BASE) return true;
    if(elem_id == AST_ELEM_GENERIC_BASE) return true;
    return false;
}

bool ast_type_is_pointer(const ast_type_t *type){
    return type->elements_length > 1 && type->elements[0]->id == AST_ELEM_POINTER;
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

bool ast_type_is_pointer_to_base(const ast_type_t *type){
    if(type->elements_length != 2) return false;
    if(type->elements[0]->id != AST_ELEM_POINTER) return false;
    
    return type->elements[1]->id == AST_ELEM_BASE;
}

bool ast_type_is_pointer_to_generic_base(const ast_type_t *type){
    if(type->elements_length != 2) return false;
    if(type->elements[0]->id != AST_ELEM_POINTER) return false;

    return type->elements[1]->id == AST_ELEM_GENERIC_BASE;
}

bool ast_type_is_pointer_to_base_like(const ast_type_t *type){
    if(type->elements_length != 2) return false;
    if(type->elements[0]->id != AST_ELEM_POINTER) return false;

    unsigned int second_elem_id = type->elements[1]->id;
    if(second_elem_id == AST_ELEM_BASE) return true;
    if(second_elem_id == AST_ELEM_GENERIC_BASE) return true;

    return false;
}

bool ast_type_is_polymorph(const ast_type_t *type){
    if(type->elements_length != 1) return false;
    if(type->elements[0]->id != AST_ELEM_POLYMORPH) return false;
    return true;
}

bool ast_type_is_polymorph_ptr(const ast_type_t *type){
    if(type->elements_length != 2) return false;
    if(type->elements[0]->id != AST_ELEM_POINTER || type->elements[1]->id != AST_ELEM_POLYMORPH) return false;
    return true;
}

bool ast_type_is_generic_base(const ast_type_t *type){
    if(type->elements_length != 1) return false;
    if(type->elements[0]->id != AST_ELEM_GENERIC_BASE) return false;
    return true;
}

bool ast_type_is_generic_base_ptr(const ast_type_t *type){
    if(type->elements_length != 2) return false;
    if(type->elements[0]->id != AST_ELEM_POINTER || type->elements[1]->id != AST_ELEM_GENERIC_BASE) return false;
    return true;
}

bool ast_type_is_fixed_array(const ast_type_t *type){
    if(type->elements_length < 2) return false;
    if(type->elements[0]->id != AST_ELEM_FIXED_ARRAY) return false;
    return true;
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
        case AST_ELEM_POLYMORPH_PREREQ:
        case AST_ELEM_POLYCOUNT:
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
            internalerrorprintf("ast_type_has_polymorph encountered unknown element id 0x%08X\n", type->elements[i]->id);
        }
    }
    return false;
}

void ast_type_dereference(ast_type_t *inout_type){
    if(inout_type->elements_length < 2 || inout_type->elements[0]->id != AST_ELEM_POINTER){
        internalerrorprintf("ast_type_dereference received non pointer type\n");
        return;
    }

    // Modify ast_type_t to remove a pointer element from the front
    // DANGEROUS: Manually deleting ast_elem_pointer_t
    free(inout_type->elements[0]);
    memmove(inout_type->elements, &inout_type->elements[1], sizeof(ast_elem_t*) * (inout_type->elements_length - 1));
    inout_type->elements_length--; // Reduce length accordingly
}

void ast_poly_catalog_init(ast_poly_catalog_t *catalog){
    catalog->types = NULL;
    catalog->types_length = 0;
    catalog->types_capacity = 0;
    catalog->counts = NULL;
    catalog->counts_length = 0;
    catalog->counts_capacity = 0;
}

void ast_poly_catalog_free(ast_poly_catalog_t *catalog){
    for(length_t i = 0; i != catalog->types_length; i++){
        ast_type_free(&catalog->types[i].binding);
    }
    free(catalog->types);
    free(catalog->counts);
}

void ast_poly_catalog_add_type(ast_poly_catalog_t *catalog, weak_cstr_t name, ast_type_t *binding){
    expand((void**) &catalog->types, sizeof(ast_poly_catalog_type_t), catalog->types_length, &catalog->types_capacity, 1, 4);
    ast_poly_catalog_type_t *type_var = &catalog->types[catalog->types_length++];
    type_var->name = name;
    type_var->binding = ast_type_clone(binding);
}

void ast_poly_catalog_add_count(ast_poly_catalog_t *catalog, weak_cstr_t name, length_t binding){
    expand((void**) &catalog->counts, sizeof(ast_poly_catalog_count_t), catalog->counts_length, &catalog->counts_capacity, 1, 4);
    ast_poly_catalog_count_t *count_var = &catalog->counts[catalog->counts_length++];
    count_var->name = name;
    count_var->binding = binding;
}

ast_poly_catalog_type_t *ast_poly_catalog_find_type(ast_poly_catalog_t *catalog, weak_cstr_t name){
    // TODO: SPEED: Improve searching (maybe not worth it?)

    for(length_t i = 0; i != catalog->types_length; i++){
        if(strcmp(catalog->types[i].name, name) == 0) return &catalog->types[i];
    }

    return NULL;
}

ast_poly_catalog_count_t *ast_poly_catalog_find_count(ast_poly_catalog_t *catalog, weak_cstr_t name){
    // TODO: SPEED: Improve searching (maybe not worth it?)

    for(length_t i = 0; i != catalog->counts_length; i++){
        if(strcmp(catalog->counts[i].name, name) == 0) return &catalog->counts[i];
    }

    return NULL;
}

hash_t ast_type_hash(ast_type_t *type){
    hash_t master_hash = 0;

    for(length_t i = 0; i != type->elements_length; i++){
        master_hash = hash_combine(master_hash, ast_elem_hash(type->elements[i]));
    }

    return master_hash;
}

hash_t ast_elem_hash(ast_elem_t *element){
    hash_t element_hash = hash_data(&element->id, sizeof(element->id));
    
    switch(element->id){
    case AST_ELEM_BASE: {
            ast_elem_base_t *base_elem = (ast_elem_base_t*) element;
            element_hash = hash_combine(element_hash, hash_data(base_elem->base, strlen(base_elem->base)));
        }
        break;
    case AST_ELEM_POINTER:
    case AST_ELEM_ARRAY:
    case AST_ELEM_GENERIC_INT:
    case AST_ELEM_GENERIC_FLOAT:
        break;
    case AST_ELEM_FIXED_ARRAY: {
            ast_elem_fixed_array_t *fixed_array_elem = (ast_elem_fixed_array_t*) element;
            element_hash = hash_combine(element_hash, hash_data(&fixed_array_elem->length, sizeof(fixed_array_elem->length)));
        }
        break;
    case AST_ELEM_FUNC: {
            ast_elem_func_t *func_elem = (ast_elem_func_t*) element;
            for(length_t i = 0; i != func_elem->arity; i++){
                element_hash = hash_combine(element_hash, ast_type_hash(&func_elem->arg_types[i]));
            }
            element_hash = hash_combine(element_hash, ast_type_hash(func_elem->return_type));
            element_hash = hash_combine(element_hash, hash_data(&func_elem->traits, sizeof(func_elem->traits)));
        }
        break;
    case AST_ELEM_POLYMORPH: {
            ast_elem_polymorph_t *polymorph = (ast_elem_polymorph_t*) element;
            element_hash = hash_combine(element_hash, hash_data(polymorph->name, strlen(polymorph->name)));
        }
        break;
    case AST_ELEM_POLYCOUNT: {
            ast_elem_polycount_t *polycount = (ast_elem_polycount_t*) element;
            element_hash = hash_combine(element_hash, hash_data("#", 1));
            element_hash = hash_combine(element_hash, hash_data(polycount->name, strlen(polycount->name)));
        }
        break;
    case AST_ELEM_POLYMORPH_PREREQ: {
            ast_elem_polymorph_prereq_t *polymorph_prereq = (ast_elem_polymorph_prereq_t*) element;
            element_hash = hash_combine(element_hash, hash_data(polymorph_prereq->name, strlen(polymorph_prereq->name)));
            element_hash = hash_combine(element_hash, hash_data(polymorph_prereq->similarity_prerequisite, strlen(polymorph_prereq->similarity_prerequisite)));
        }
        break;
    case AST_ELEM_GENERIC_BASE: {
            ast_elem_generic_base_t *generic_base_elem = (ast_elem_generic_base_t*) element;
            element_hash = hash_combine(element_hash, hash_data(generic_base_elem->name, strlen(generic_base_elem->name)));

            for(length_t i = 0; i != generic_base_elem->generics_length; i++){
                element_hash = hash_combine(element_hash, ast_type_hash(&generic_base_elem->generics[i]));
            }

            element_hash = hash_combine(element_hash, hash_data(&generic_base_elem->name_is_polymorphic, sizeof(generic_base_elem->name_is_polymorphic)));
        }
        break;
    default:
        internalerrorprintf("Encountered unexpected type element id when cloning ast_elem_t, a crash will probably follow...\n");
    }

    return element_hash;
}
