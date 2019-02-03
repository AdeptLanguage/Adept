
#include "AST/ast.h"
#include "AST/ast_type.h"
#include "UTIL/color.h"

ast_type_t ast_type_clone(const ast_type_t *original){
    ast_type_t new;
    new.elements = malloc(sizeof(ast_elem_t*) * original->elements_length);
    new.elements_length = original->elements_length;
    new.source = original->source;

    for(length_t i = 0; i != original->elements_length; i++){
        switch(original->elements[i]->id){
        case AST_ELEM_BASE: {
                char *original_base = ((ast_elem_base_t*) original->elements[i])->base;
                length_t original_base_length = strlen(original_base);
                char *new_base = malloc(original_base_length + 1);
                memcpy(new_base, original_base, original_base_length + 1);
                new.elements[i] = malloc(sizeof(ast_elem_base_t));
                ((ast_elem_base_t*) new.elements[i])->id = AST_ELEM_BASE;
                ((ast_elem_base_t*) new.elements[i])->source = original->elements[i]->source;
                ((ast_elem_base_t*) new.elements[i])->base = new_base;
                break;
            }
        case AST_ELEM_POINTER:
            new.elements[i] = malloc(sizeof(ast_elem_pointer_t));
            ((ast_elem_pointer_t*) new.elements[i])->id = AST_ELEM_POINTER;
            ((ast_elem_pointer_t*) new.elements[i])->source = original->elements[i]->source;
            break;
        case AST_ELEM_ARRAY:
            new.elements[i] = malloc(sizeof(ast_elem_array_t));
            ((ast_elem_array_t*) new.elements[i])->id = AST_ELEM_ARRAY;
            ((ast_elem_array_t*) new.elements[i])->source = original->elements[i]->source;
            break;
        case AST_ELEM_FIXED_ARRAY:
            new.elements[i] = malloc(sizeof(ast_elem_fixed_array_t));
            ((ast_elem_fixed_array_t*) new.elements[i])->id = AST_ELEM_FIXED_ARRAY;
            ((ast_elem_fixed_array_t*) new.elements[i])->source = original->elements[i]->source;
            ((ast_elem_fixed_array_t*) new.elements[i])->length = ((ast_elem_fixed_array_t*) original->elements[i])->length;
            break;
        case AST_ELEM_GENERIC_INT:
            new.elements[i] = malloc(sizeof(ast_elem_t));
            new.elements[i]->id = AST_ELEM_GENERIC_INT;
            new.elements[i]->source = original->elements[i]->source;
            break;
        case AST_ELEM_GENERIC_FLOAT:
            new.elements[i] = malloc(sizeof(ast_elem_t));
            new.elements[i]->id = AST_ELEM_GENERIC_FLOAT;
            new.elements[i]->source = original->elements[i]->source;
            break;
        case AST_ELEM_FUNC:
            new.elements[i] = malloc(sizeof(ast_elem_func_t));
            ((ast_elem_func_t*) new.elements[i])->id = AST_ELEM_FUNC;
            ((ast_elem_func_t*) new.elements[i])->source = ((ast_elem_func_t*) original->elements[i])->source;
            ((ast_elem_func_t*) new.elements[i])->arg_types = ((ast_elem_func_t*) original->elements[i])->arg_types;
            ((ast_elem_func_t*) new.elements[i])->arg_flows = ((ast_elem_func_t*) original->elements[i])->arg_flows;
            ((ast_elem_func_t*) new.elements[i])->arity = ((ast_elem_func_t*) original->elements[i])->arity;
            ((ast_elem_func_t*) new.elements[i])->return_type = ((ast_elem_func_t*) original->elements[i])->return_type;
            ((ast_elem_func_t*) new.elements[i])->traits = ((ast_elem_func_t*) original->elements[i])->traits;
            ((ast_elem_func_t*) new.elements[i])->ownership = false;
            break;
        default:
            redprintf("INTERNAL ERROR: Encountered unexpected type element id when cloning ast_type_t, a crash will probably follow...\n");
        }
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
            if( ((ast_elem_func_t*) type->elements[i])->ownership ){
                ast_elem_func_t *func_elem = (ast_elem_func_t*) type->elements[i];
                ast_types_free(func_elem->arg_types, func_elem->arity);
                free(func_elem->arg_types);
                free(func_elem->arg_flows);
                ast_type_free_fully(func_elem->return_type);
            }
            free(type->elements[i]);
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
