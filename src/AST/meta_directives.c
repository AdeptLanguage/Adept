
#include <math.h>
#include "LEX/lex.h"
#include "UTIL/util.h"
#include "UTIL/color.h"
#include "UTIL/search.h"
#include "DRVR/compiler.h"
#include "AST/meta_directives.h"

strong_cstr_t meta_expr_str(meta_expr_t *meta){
    switch(meta->id){
    case META_EXPR_UNDEF:
        return strclone("undef");
    case META_EXPR_NULL:
        return strclone("null");
    case META_EXPR_TRUE:
        return strclone("true");
    case META_EXPR_FALSE:
        return strclone("false");
    case META_EXPR_STR:
        return strclone(((meta_expr_str_t*) meta)->value);
    case META_EXPR_INT: {
            strong_cstr_t representation = malloc(21);
            sprintf(representation, "%ld", (long) ((meta_expr_int_t*) meta)->value);
            return representation;
        }
    case META_EXPR_FLOAT: {
            strong_cstr_t representation = malloc(21);
            sprintf(representation, "%06.6f", ((meta_expr_float_t*) meta)->value);
            return representation;
        }
    default:
        redprintf("INTERNAL ERROR: Tried to get string from non-collapsed meta expression!\n");
        redprintf("A crash will probably follow...\n");
        return NULL;
    }
}

void meta_expr_free(meta_expr_t *expr){
    switch(expr->id){
    case META_EXPR_UNDEF:
    case META_EXPR_NULL:
    case META_EXPR_TRUE:
    case META_EXPR_FALSE:
    case META_EXPR_INT:
    case META_EXPR_FLOAT:
        break;
    case META_EXPR_AND:
    case META_EXPR_OR:
    case META_EXPR_XOR:
    case META_EXPR_ADD:
    case META_EXPR_SUB:
    case META_EXPR_MUL:
    case META_EXPR_DIV:
    case META_EXPR_MOD:
    case META_EXPR_POW:
    case META_EXPR_EQ:
    case META_EXPR_NEQ:
    case META_EXPR_GT:
    case META_EXPR_GTE:
    case META_EXPR_LT:
    case META_EXPR_LTE: {
            meta_expr_math_t *math = (meta_expr_math_t*) expr;
            meta_expr_free_fully(math->a);
            meta_expr_free_fully(math->b);
        }
        break;
    case META_EXPR_STR: {
            meta_expr_str_t *str = (meta_expr_str_t*) expr;
            free(str->value);
        }
        break;
    case META_EXPR_VAR: {
            meta_expr_var_t *str = (meta_expr_var_t*) expr;
            free(str->name);
        }
        break;
    case META_EXPR_NOT:
        meta_expr_free_fully(((meta_expr_not_t*) expr)->value);
        break;
    default:
        redprintf("INTERNAL ERROR: meta_expr_free encountered unknown meta expression id %d!\n", (int) expr->id);
    }
}

void meta_expr_free_fully(meta_expr_t *expr){
    meta_expr_free(expr);
    free(expr);
}

meta_expr_t *meta_expr_clone(meta_expr_t *expr){
    switch(expr->id){
    case META_EXPR_UNDEF: case META_EXPR_NULL: case META_EXPR_TRUE: case META_EXPR_FALSE: {
            meta_expr_t *result = malloc(sizeof(meta_expr_t));
            result->id = expr->id;
            return result;
        }
    case META_EXPR_STR: {
            meta_expr_str_t *result = malloc(sizeof(meta_expr_str_t));
            result->id = META_EXPR_STR;
            result->value = strclone(((meta_expr_str_t*) expr)->value);
            return (meta_expr_t*) result;
        }
    case META_EXPR_INT: {
            meta_expr_int_t *result = malloc(sizeof(meta_expr_int_t));
            result->id = META_EXPR_INT;
            result->value = ((meta_expr_int_t*) expr)->value;
            return (meta_expr_t*) result;
        }
    case META_EXPR_FLOAT: {
            meta_expr_float_t *result = malloc(sizeof(meta_expr_float_t));
            result->id = META_EXPR_FLOAT;
            result->value = ((meta_expr_float_t*) expr)->value;
            return (meta_expr_t*) result;
        }
    default:
        redprintf("INTERNAL ERROR: Tried to clone a non-collapsed meta expression!\n");
        redprintf("A crash will probably follow...\n");
        return NULL;
    }
}

errorcode_t meta_collapse(compiler_t *compiler, object_t *object, meta_definition_t *definitions, length_t definitions_length, meta_expr_t **expr){
    #define META_EXPR_MATH_MODE_INT   0x00
    #define META_EXPR_MATH_MODE_FLOAT 0x01
    #define META_EXPR_MATH_MODE_STR   0x02

    typedef char meta_math_modes_t[3][3];
    
    while(!IS_META_EXPR_ID_COLLAPSED((*expr)->id)){
        switch((*expr)->id){
        case META_EXPR_AND: {
                meta_expr_and_t *and_expr = (meta_expr_and_t*) *expr;
                bool a, b;
                if(meta_expr_into_bool(compiler, object, definitions, definitions_length, &and_expr->a, &a)) return FAILURE;
                if(meta_expr_into_bool(compiler, object, definitions, definitions_length, &and_expr->b, &b)) return FAILURE;
                meta_expr_free_fully(and_expr->a);
                meta_expr_free_fully(and_expr->b);
                (*expr)->id = (a && b) ? META_EXPR_TRUE : META_EXPR_FALSE;
            }
            break;
        case META_EXPR_OR: {
                meta_expr_or_t *or_expr = (meta_expr_or_t*) *expr;
                bool a, b;
                if(meta_expr_into_bool(compiler, object, definitions, definitions_length, &or_expr->a, &a)) return FAILURE;
                if(meta_expr_into_bool(compiler, object, definitions, definitions_length, &or_expr->b, &b)) return FAILURE;
                meta_expr_free_fully(or_expr->a);
                meta_expr_free_fully(or_expr->b);
                (*expr)->id = (a || b) ? META_EXPR_TRUE : META_EXPR_FALSE;
            }
            break;
        case META_EXPR_XOR: {
                meta_expr_xor_t *xor_expr = (meta_expr_xor_t*) *expr;
                bool a, b;
                if(meta_expr_into_bool(compiler, object, definitions, definitions_length, &xor_expr->a, &a)) return FAILURE;
                if(meta_expr_into_bool(compiler, object, definitions, definitions_length, &xor_expr->b, &b)) return FAILURE;
                meta_expr_free_fully(xor_expr->a);
                meta_expr_free_fully(xor_expr->b);
                (*expr)->id = (a ^ b) ? META_EXPR_TRUE : META_EXPR_FALSE;
            }
            break;
        case META_EXPR_VAR: {
                meta_expr_var_t *var = (meta_expr_var_t*) *expr;
                meta_expr_t *special_result = meta_get_special_variable(compiler, object, var->name, var->source);

                if(special_result){
                    meta_expr_free_fully(*expr);
                    *expr = special_result;
                    break;
                }
                
                meta_definition_t *definition = meta_definition_find(definitions, definitions_length, var->name);

                if(definition){
                    meta_expr_free_fully(*expr);
                    *expr = meta_expr_clone(definition->value);
                } else {
                    if(!(compiler->traits & COMPILER_UNSAFE_META || compiler->traits & COMPILER_NO_WARN)){
                        bool should_exit = compiler_warnf(compiler, var->source, "Warning: Usage of undefined transcendant variable '%s'", var->name);
                        printf("    (you can disable this warning with '--unsafe-meta' or 'pragma unsafe_meta')\n");

                        if(should_exit){
                            meta_expr_free_fully(*expr);
                            return FAILURE;
                        }
                    }

                    meta_expr_free(*expr);
                    (*expr)->id = META_EXPR_UNDEF;
                }
            }
            break;
        case META_EXPR_ADD: case META_EXPR_SUB: case META_EXPR_MUL:
        case META_EXPR_DIV: case META_EXPR_MOD: case META_EXPR_POW: {
                unsigned int math_expr_id = (*expr)->id;

                meta_expr_math_t *math_expr = (meta_expr_math_t*) *expr;
                if(meta_collapse(compiler, object, definitions, definitions_length, &math_expr->a)) return FAILURE;
                if(meta_collapse(compiler, object, definitions, definitions_length, &math_expr->b)) return FAILURE;

                //                    B
                //           i        d         s
                //        -------------------------
                //     i |   i        d         i
                //       |
                // A   d |   d        d         d
                //       |
                //     s |   s        s         s
                //       |
                static meta_math_modes_t addition_modes = {
                                                            /* b */
                        {META_EXPR_MATH_MODE_INT,   META_EXPR_MATH_MODE_FLOAT, META_EXPR_MATH_MODE_INT},
                /* a */ {META_EXPR_MATH_MODE_FLOAT, META_EXPR_MATH_MODE_FLOAT, META_EXPR_MATH_MODE_FLOAT},
                        {META_EXPR_MATH_MODE_STR,   META_EXPR_MATH_MODE_STR,   META_EXPR_MATH_MODE_STR}
                };

                static meta_math_modes_t typical_modes = {
                                                            /* b */
                        {META_EXPR_MATH_MODE_INT,   META_EXPR_MATH_MODE_FLOAT, META_EXPR_MATH_MODE_INT},
                /* a */ {META_EXPR_MATH_MODE_FLOAT, META_EXPR_MATH_MODE_FLOAT, META_EXPR_MATH_MODE_FLOAT},
                        {META_EXPR_MATH_MODE_INT,   META_EXPR_MATH_MODE_FLOAT, META_EXPR_MATH_MODE_FLOAT}
                };

                static meta_math_modes_t power_modes = {
                                                            /* b */
                        {META_EXPR_MATH_MODE_FLOAT, META_EXPR_MATH_MODE_FLOAT, META_EXPR_MATH_MODE_FLOAT},
                /* a */ {META_EXPR_MATH_MODE_FLOAT, META_EXPR_MATH_MODE_FLOAT, META_EXPR_MATH_MODE_FLOAT},
                        {META_EXPR_MATH_MODE_FLOAT, META_EXPR_MATH_MODE_FLOAT, META_EXPR_MATH_MODE_FLOAT}
                };

                char a_mode, b_mode;

                switch(math_expr->a->id){
                case META_EXPR_INT:   a_mode = META_EXPR_MATH_MODE_INT;   break;
                case META_EXPR_FLOAT: a_mode = META_EXPR_MATH_MODE_FLOAT; break;
                case META_EXPR_STR:   a_mode = META_EXPR_MATH_MODE_STR;   break;
                default: a_mode = META_EXPR_MATH_MODE_INT;
                }

                switch(math_expr->b->id){
                case META_EXPR_INT:   b_mode = META_EXPR_MATH_MODE_INT;   break;
                case META_EXPR_FLOAT: b_mode = META_EXPR_MATH_MODE_FLOAT; break;
                case META_EXPR_STR:   b_mode = META_EXPR_MATH_MODE_STR;   break;
                default: b_mode = META_EXPR_MATH_MODE_INT;
                }

                // *(modes_for_operation[math_expr_id - META_EXPR_ADD]) == <mode for that expr id>
                static meta_math_modes_t *modes_for_operation[] = {
                    &addition_modes, // META_EXPR_ADD   0x0A
                    &typical_modes,  // META_EXPR_SUB   0x0B
                    &typical_modes,  // META_EXPR_MUL   0x0C
                    &typical_modes,  // META_EXPR_DIV   0x0D
                    &typical_modes,  // META_EXPR_MOD   0x0E
                    &power_modes     // META_EXPR_POW   0x0F
                };

                char mode = (*modes_for_operation[math_expr_id - META_EXPR_ADD])[(int) a_mode][(int) b_mode];
                
                switch(mode){
                case META_EXPR_MATH_MODE_INT: {
                        long long a_int, b_int;
                        if(meta_expr_into_int(compiler, object, definitions, definitions_length, &math_expr->a, &a_int)) return FAILURE;
                        if(meta_expr_into_int(compiler, object, definitions, definitions_length, &math_expr->b, &b_int)) return FAILURE;

                        meta_expr_int_t *result = malloc(sizeof(meta_expr_int_t));
                        result->id = META_EXPR_INT;

                        switch(math_expr_id){
                        case META_EXPR_ADD: result->value = a_int + b_int; break;
                        case META_EXPR_SUB: result->value = a_int - b_int; break;
                        case META_EXPR_MUL: result->value = a_int * b_int; break;
                        case META_EXPR_DIV: result->value = a_int / b_int; break;
                        case META_EXPR_MOD: result->value = a_int % b_int; break;
                        default:
                            result->value = 0;
                        }

                        meta_expr_free_fully(*expr);
                        *expr = (meta_expr_t*) result;
                    }
                    break;
                case META_EXPR_MATH_MODE_FLOAT: {
                        double a_float, b_float;
                        if(meta_expr_into_float(compiler, object, definitions, definitions_length, &math_expr->a, &a_float)) return FAILURE;
                        if(meta_expr_into_float(compiler, object, definitions, definitions_length, &math_expr->b, &b_float)) return FAILURE;

                        meta_expr_float_t *result = malloc(sizeof(meta_expr_float_t));
                        result->id = META_EXPR_FLOAT;

                        switch(math_expr_id){
                        case META_EXPR_ADD: result->value = a_float + b_float;      break;
                        case META_EXPR_SUB: result->value = a_float - b_float;      break;
                        case META_EXPR_MUL: result->value = a_float * b_float;      break;
                        case META_EXPR_DIV: result->value = a_float / b_float;      break;
                        case META_EXPR_MOD: result->value = fmod(a_float, b_float); break;
                        case META_EXPR_POW: result->value = pow(a_float, b_float);  break;
                        default:
                            result->value = 0;
                        }

                        meta_expr_free_fully(*expr);
                        *expr = (meta_expr_t*) result;
                    }
                    break;
                case META_EXPR_MATH_MODE_STR: {
                        strong_cstr_t a_str, b_str;
                        if(meta_expr_into_string(compiler, object, definitions, definitions_length, &math_expr->a, &a_str)) return FAILURE;

                        if(meta_expr_into_string(compiler, object, definitions, definitions_length, &math_expr->b, &b_str)){
                            free(a_str);
                            return FAILURE;
                        }

                        length_t a_len = strlen(a_str);
                        length_t b_len = strlen(b_str);

                        strong_cstr_t concat = malloc(a_len + b_len + 1);
                        memcpy(concat, a_str, a_len);
                        memcpy(&concat[a_len], b_str, b_len + 1);

                        free(a_str);
                        free(b_str);

                        meta_expr_str_t *result = malloc(sizeof(meta_expr_str_t));
                        result->id = META_EXPR_STR;
                        result->value = concat;

                        meta_expr_free_fully(*expr);
                        *expr = (meta_expr_t*) result;
                    }
                    break;
                }
            }
            break;
        case META_EXPR_EQ: case META_EXPR_NEQ: case META_EXPR_GT:
        case META_EXPR_GTE: case META_EXPR_LT: case META_EXPR_LTE: {
                unsigned int math_expr_id = (*expr)->id;

                meta_expr_math_t *math_expr = (meta_expr_math_t*) *expr;
                if(meta_collapse(compiler, object, definitions, definitions_length, &math_expr->a)) return FAILURE;
                if(meta_collapse(compiler, object, definitions, definitions_length, &math_expr->b)) return FAILURE;

                static meta_math_modes_t comparison_modes = {
                                                            /* b */
                        {META_EXPR_MATH_MODE_INT,   META_EXPR_MATH_MODE_FLOAT, META_EXPR_MATH_MODE_INT},
                /* a */ {META_EXPR_MATH_MODE_FLOAT, META_EXPR_MATH_MODE_FLOAT, META_EXPR_MATH_MODE_FLOAT},
                        {META_EXPR_MATH_MODE_INT,   META_EXPR_MATH_MODE_FLOAT, META_EXPR_MATH_MODE_STR}
                };

                char a_mode, b_mode;

                switch(math_expr->a->id){
                case META_EXPR_INT:   a_mode = META_EXPR_MATH_MODE_INT;   break;
                case META_EXPR_FLOAT: a_mode = META_EXPR_MATH_MODE_FLOAT; break;
                case META_EXPR_STR:   a_mode = META_EXPR_MATH_MODE_STR;   break;
                default: a_mode = META_EXPR_MATH_MODE_INT;
                }

                switch(math_expr->b->id){
                case META_EXPR_INT:   b_mode = META_EXPR_MATH_MODE_INT;   break;
                case META_EXPR_FLOAT: b_mode = META_EXPR_MATH_MODE_FLOAT; break;
                case META_EXPR_STR:   b_mode = META_EXPR_MATH_MODE_STR;   break;
                default: b_mode = META_EXPR_MATH_MODE_INT;
                }

                char mode = comparison_modes[(int) a_mode][(int) b_mode];
                meta_expr_int_t *result = malloc(sizeof(meta_expr_t));

                switch(mode){
                case META_EXPR_MATH_MODE_INT: {
                        long long a_int, b_int;
                        if(meta_expr_into_int(compiler, object, definitions, definitions_length, &math_expr->a, &a_int)) return FAILURE;
                        if(meta_expr_into_int(compiler, object, definitions, definitions_length, &math_expr->b, &b_int)) return FAILURE;

                        switch(math_expr_id){
                        case META_EXPR_EQ:  result->id = (a_int == b_int) ? META_EXPR_TRUE : META_EXPR_FALSE; break;
                        case META_EXPR_NEQ: result->id = (a_int != b_int) ? META_EXPR_TRUE : META_EXPR_FALSE; break;
                        case META_EXPR_GT:  result->id = (a_int  > b_int) ? META_EXPR_TRUE : META_EXPR_FALSE; break;
                        case META_EXPR_GTE: result->id = (a_int >= b_int) ? META_EXPR_TRUE : META_EXPR_FALSE; break;
                        case META_EXPR_LT:  result->id = (a_int  < b_int) ? META_EXPR_TRUE : META_EXPR_FALSE; break;
                        case META_EXPR_LTE: result->id = (a_int <= b_int) ? META_EXPR_TRUE : META_EXPR_FALSE; break;
                        default:
                            result->id = META_EXPR_FALSE;
                        }

                        meta_expr_free_fully(*expr);
                        *expr = (meta_expr_t*) result;
                    }
                    break;
                case META_EXPR_MATH_MODE_FLOAT: {
                        double a_float, b_float;
                        if(meta_expr_into_float(compiler, object, definitions, definitions_length, &math_expr->a, &a_float)) return FAILURE;
                        if(meta_expr_into_float(compiler, object, definitions, definitions_length, &math_expr->b, &b_float)) return FAILURE;

                        switch(math_expr_id){
                        case META_EXPR_EQ:  result->id = (a_float == b_float) ? META_EXPR_TRUE : META_EXPR_FALSE; break;
                        case META_EXPR_NEQ: result->id = (a_float != b_float) ? META_EXPR_TRUE : META_EXPR_FALSE; break;
                        case META_EXPR_GT:  result->id = (a_float  > b_float) ? META_EXPR_TRUE : META_EXPR_FALSE; break;
                        case META_EXPR_GTE: result->id = (a_float >= b_float) ? META_EXPR_TRUE : META_EXPR_FALSE; break;
                        case META_EXPR_LT:  result->id = (a_float  < b_float) ? META_EXPR_TRUE : META_EXPR_FALSE; break;
                        case META_EXPR_LTE: result->id = (a_float <= b_float) ? META_EXPR_TRUE : META_EXPR_FALSE; break;
                        default:
                            result->id = META_EXPR_FALSE;
                        }

                        meta_expr_free_fully(*expr);
                        *expr = (meta_expr_t*) result;
                    }
                    break;
                case META_EXPR_MATH_MODE_STR: {
                        strong_cstr_t a_str, b_str;
                        if(meta_expr_into_string(compiler, object, definitions, definitions_length, &math_expr->a, &a_str)) return FAILURE;

                        if(meta_expr_into_string(compiler, object, definitions, definitions_length, &math_expr->b, &b_str)){
                            free(a_str);
                            return FAILURE;
                        }

                        switch(math_expr_id){
                        case META_EXPR_EQ:  result->id = strcmp(a_str, b_str) == 0 ? META_EXPR_TRUE : META_EXPR_FALSE; break;
                        case META_EXPR_NEQ: result->id = strcmp(a_str, b_str) != 0 ? META_EXPR_TRUE : META_EXPR_FALSE; break;
                        case META_EXPR_GT:  result->id = strcmp(a_str, b_str)  > 0 ? META_EXPR_TRUE : META_EXPR_FALSE; break;
                        case META_EXPR_GTE: result->id = strcmp(a_str, b_str) >= 0 ? META_EXPR_TRUE : META_EXPR_FALSE; break;
                        case META_EXPR_LT:  result->id = strcmp(a_str, b_str)  < 0 ? META_EXPR_TRUE : META_EXPR_FALSE; break;
                        case META_EXPR_LTE: result->id = strcmp(a_str, b_str) <= 0 ? META_EXPR_TRUE : META_EXPR_FALSE; break;
                        default:
                            result->id = META_EXPR_FALSE;
                        }

                        free(a_str);
                        free(b_str);
                        meta_expr_free_fully(*expr);
                        *expr = (meta_expr_t*) result;
                    }
                    break;
                default:
                    result->id = META_EXPR_FALSE;
                    meta_expr_free_fully(*expr);
                    *expr = (meta_expr_t*) result;
                }
            }
            break;
        case META_EXPR_NOT: {
                meta_expr_not_t *not_expr = (meta_expr_not_t*) *expr;
                bool is_true;

                if(meta_expr_into_bool(compiler, object, definitions, definitions_length, &not_expr->value, &is_true))
                    return FAILURE;
                
                meta_expr_free_fully(not_expr->value);
                (*expr)->id = is_true ? META_EXPR_FALSE : META_EXPR_TRUE;
            }
            break;
        default:
            redprintf("INTERNAL ERROR: Unrecognized meta expression in meta_collapse!\n");
            return FAILURE;
        }
    }

    return SUCCESS;
}

errorcode_t meta_expr_into_bool(compiler_t *compiler, object_t *object, meta_definition_t *definitions, length_t definitions_length, meta_expr_t **expr, bool *out){
    if(meta_collapse(compiler, object, definitions, definitions_length, expr)) return FAILURE;

    switch((*expr)->id){
    case META_EXPR_UNDEF:
        *out = false;
        break;
    case META_EXPR_NULL:
        *out = false;
        break;
    case META_EXPR_TRUE:
        *out = true;
        break;
    case META_EXPR_FALSE:
        *out = false;
        break;
    case META_EXPR_INT:
        *out = ((meta_expr_int_t*) *expr)->value != 0;
        break;
    case META_EXPR_FLOAT:
        *out = ((meta_expr_float_t*) *expr)->value != 0;
        break;
    default:
        *out = true;
    }

    return SUCCESS;
}

errorcode_t meta_expr_into_int(compiler_t *compiler, object_t *object, meta_definition_t *definitions, length_t definitions_length, meta_expr_t **expr, long long *out){
    if(meta_collapse(compiler, object, definitions, definitions_length, expr)) return FAILURE;

    switch((*expr)->id){
    case META_EXPR_UNDEF:
        *out = 0;
        break;
    case META_EXPR_NULL:
        *out = 0;
        break;
    case META_EXPR_TRUE:
        *out = 1;
        break;
    case META_EXPR_FALSE:
        *out = 0;
        break;
    case META_EXPR_INT:
        *out = ((meta_expr_int_t*) *expr)->value;
        break;
    case META_EXPR_FLOAT:
        *out = (long long) ((meta_expr_float_t*) *expr)->value;
        break;
    case META_EXPR_STR:
        *out = atoll(((meta_expr_str_t*) *expr)->value);
        break;
    default:
        *out = 0;
    }

    return SUCCESS;
}

errorcode_t meta_expr_into_float(compiler_t *compiler, object_t *object, meta_definition_t *definitions, length_t definitions_length, meta_expr_t **expr, double *out){
    if(meta_collapse(compiler, object, definitions, definitions_length, expr)) return FAILURE;

    switch((*expr)->id){
    case META_EXPR_UNDEF:
        *out = 0.0;
        break;
    case META_EXPR_NULL:
        *out = 0.0;
        break;
    case META_EXPR_TRUE:
        *out = 1.0;
        break;
    case META_EXPR_FALSE:
        *out = 0.0;
        break;
    case META_EXPR_INT:
        *out = (double) ((meta_expr_int_t*) *expr)->value;
        break;
    case META_EXPR_FLOAT:
        *out = ((meta_expr_float_t*) *expr)->value;
        break;
    case META_EXPR_STR:
        *out = atof(((meta_expr_str_t*) *expr)->value);
        break;
    default:
        *out = 0.0;
    }

    return SUCCESS;
}

errorcode_t meta_expr_into_string(compiler_t *compiler, object_t *object, meta_definition_t *definitions, length_t definitions_length, meta_expr_t **expr, strong_cstr_t *out){
    if(meta_collapse(compiler, object, definitions, definitions_length, expr)) return FAILURE;

    switch((*expr)->id){
    case META_EXPR_UNDEF:
        *out = strclone("undef");
        break;
    case META_EXPR_NULL:
        *out = strclone("null");
        break;
    case META_EXPR_TRUE:
        *out = strclone("true");
        break;
    case META_EXPR_FALSE:
        *out = strclone("false");
        break;
    case META_EXPR_INT: {
            strong_cstr_t representation = malloc(21);
            sprintf(representation, "%ld", (long) ((meta_expr_int_t*) *expr)->value);
            *out = representation;
        }
        break;
    case META_EXPR_FLOAT: {
            strong_cstr_t representation = malloc(21);
            sprintf(representation, "%06.6f", ((meta_expr_float_t*) *expr)->value);
            *out = representation;
        }
        break;
    case META_EXPR_STR:
        *out = strclone(((meta_expr_str_t*) *expr)->value);
        break;
    default:
        *out = strclone("");
    }

    return SUCCESS;
}

void meta_definition_add(meta_definition_t **definitions, length_t *length, length_t *capacity, weak_cstr_t name, meta_expr_t *value){
    expand((void**) definitions, sizeof(meta_definition_t), *length, capacity, 1, 8);
    (*definitions)[*length].name = name;
    (*definitions)[(*length)++].value = value;
}

void meta_definition_add_bool(meta_definition_t **definitions, length_t *length, length_t *capacity, weak_cstr_t name, bool boolean){
    meta_expr_t *value = malloc(sizeof(meta_expr_t));
    value->id = boolean ? META_EXPR_TRUE : META_EXPR_FALSE;
    meta_definition_add(definitions, length, capacity, name, value);
}

void meta_definition_add_str(meta_definition_t **definitions, length_t *length, length_t *capacity, weak_cstr_t name, strong_cstr_t content){
    meta_expr_str_t *value = (meta_expr_str_t*) malloc(sizeof(meta_expr_str_t));
    value->id = META_EXPR_STR;
    value->value = content;
    meta_definition_add(definitions, length, capacity, name, (meta_expr_t*) value);
}

meta_definition_t *meta_definition_find(meta_definition_t *definitions, length_t length, weak_cstr_t name){
    for(length_t i = 0; i != length; i++){
        if(strcmp(definitions[i].name, name) == 0) return &definitions[i];
    }
    return NULL;
}

meta_expr_t *meta_get_special_variable(compiler_t *compiler, object_t *object, weak_cstr_t variable_name, source_t variable_source){
    meta_expr_t *result;

    // Array of dynamic compiler meta variable names
    // NOTE: This list must be sorted alphabetically
    static const char* special_dynamic_meta_variables[] = {
        "__column__",
        "__file__",
        "__line__",
        "__no_typeinfo__",
        "__randmax__",
        "__stdlib__",
        "__typeinfo__"
    };

    length_t special_dynamic_meta_variables_length = sizeof(special_dynamic_meta_variables) / sizeof(const char *);
    maybe_index_t special_index = binary_string_search(special_dynamic_meta_variables, special_dynamic_meta_variables_length, variable_name);

    // Do special stuff instead if dynamic compiler meta variable
    if(special_index < 0) return NULL;

    switch(special_index){
    case 0: { // __column__
            int line, column;
            lex_get_location(compiler->objects[variable_source.object_index]->buffer, variable_source.index, &line, &column);
            
            result = malloc(sizeof(meta_expr_int_t));
            ((meta_expr_int_t*) result)->id = META_EXPR_INT;
            ((meta_expr_int_t*) result)->value = column;
        }
        break;
    case 1: { // __file__
            result = malloc(sizeof(meta_expr_str_t));
            ((meta_expr_str_t*) result)->id = META_EXPR_STR;
            ((meta_expr_str_t*) result)->value = strclone(compiler->objects[variable_source.object_index]->filename);
        }
        break;
    case 2: { // __line__
            int line, column;
            lex_get_location(compiler->objects[variable_source.object_index]->buffer, variable_source.index, &line, &column);
            
            result = malloc(sizeof(meta_expr_int_t));
            ((meta_expr_int_t*) result)->id = META_EXPR_INT;
            ((meta_expr_int_t*) result)->value = line;
        }
        break;
    case 3: // __no_typeinfo__
        result = malloc(sizeof(meta_expr_t));
        result->id = compiler->traits & COMPILER_NO_TYPEINFO ? META_EXPR_TRUE : META_EXPR_FALSE;
        break;
    case 4: { // __randmax__
            result = malloc(sizeof(meta_expr_int_t));
            ((meta_expr_int_t*) result)->id = META_EXPR_INT;
            ((meta_expr_int_t*) result)->value = RAND_MAX;
        }
        break;
    case 5: // __stdlib__
        result = malloc(sizeof(meta_expr_str_t));
        ((meta_expr_str_t*) result)->id = META_EXPR_STR;
        ((meta_expr_str_t*) result)->value = compiler_get_stdlib(compiler, object);
        break;
    case 6: // __typeinfo__
        result = malloc(sizeof(meta_expr_t));
        result->id = compiler->traits & COMPILER_NO_TYPEINFO ? META_EXPR_FALSE : META_EXPR_TRUE;
        break;
    default:
        compiler_panic(compiler, variable_source, "INTERNAL ERROR: meta_get_special_variable() got unimplemented index");
        return NULL;
    }
    
    return result;
}
