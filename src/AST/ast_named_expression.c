
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "AST/ast_expr.h"
#include "AST/ast_named_expression.h"
#include "UTIL/ground.h"
#include "UTIL/string.h"
#include "UTIL/trait.h"
#include "UTIL/util.h"

ast_named_expression_t ast_named_expression_create(strong_cstr_t name, ast_expr_t *value, trait_t traits, source_t source){
    return (ast_named_expression_t){
        .name = name,
        .expression = value,
        .traits = traits,
        .source = source,
    };
}

void ast_named_expression_free(ast_named_expression_t *named_expression){
    ast_expr_free_fully(named_expression->expression);
    free(named_expression->name);
}

ast_named_expression_t ast_named_expression_clone(ast_named_expression_t *original){
    return (ast_named_expression_t){
        .name = strclone(original->name),
        .expression = ast_expr_clone(original->expression),
        .traits = original->traits,
        .source = original->source,
    };
}

// ---

void ast_named_expression_list_free(ast_named_expression_list_t *list){
    for(length_t i = 0; i != list->length; i++){
        ast_named_expression_free(&list->expressions[i]);
    }

    free(list->expressions);
}

ast_named_expression_t *ast_named_expression_list_find(ast_named_expression_list_t *sorted_list, const char *name){
    maybe_index_t index = ast_named_expression_list_find_index(sorted_list, name);

    if(index != -1){
        return &sorted_list->expressions[index];
    }

    return NULL;
}

maybe_index_t ast_named_expression_list_find_index(ast_named_expression_list_t *sorted_list, const char *name){
    ast_named_expression_t *sorted_named_expressions = sorted_list->expressions;
    length_t length = sorted_list->length;

    maybe_index_t first, middle, last, comparison;
    first = 0; last = (maybe_index_t)length - 1;

    while(first <= last){
        middle = (first + last) / 2;
        comparison = strcmp(sorted_named_expressions[middle].name, name);

        if(comparison == 0) return middle;
        else if(comparison > 0) last = middle - 1;
        else first = middle + 1;
    }

    return -1;
}

void ast_named_expression_list_insert_sorted(ast_named_expression_list_t *sorted_list, ast_named_expression_t named_expression){
    expand((void**) &sorted_list->expressions, sizeof(ast_named_expression_t), sorted_list->length, &sorted_list->capacity, 1, 4);

    length_t insert_position = find_insert_position(sorted_list->expressions, sorted_list->length, ast_named_expressions_cmp, &named_expression, sizeof(ast_named_expression_t));

    // Move other named expressions over, so that the list will remain sorted
    memmove(&sorted_list->expressions[insert_position + 1], &sorted_list->expressions[insert_position], sizeof(ast_named_expression_t) * (sorted_list->length - insert_position));

    sorted_list->expressions[insert_position] = named_expression;
    sorted_list->length++;
}

void ast_named_expression_list_sort(ast_named_expression_list_t *list){
    qsort(list->expressions, list->length, sizeof(ast_named_expression_t), ast_named_expressions_cmp);
}

// ---

int ast_named_expressions_cmp(const void *a, const void *b){
    return strcmp(((ast_named_expression_t*) a)->name, ((ast_named_expression_t*) b)->name);
}
