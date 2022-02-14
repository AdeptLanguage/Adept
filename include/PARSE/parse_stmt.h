
#ifndef _ISAAC_PARSE_STMT_H
#define _ISAAC_PARSE_STMT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "UTIL/ground.h"
#include "AST/ast_expr.h"
#include "PARSE/parse_ctx.h"

// ------------------ defer_scope_t ------------------
// A single scope for containing defer statements
struct defer_scope;
#define BREAKABLE       TRAIT_1
#define CONTINUABLE     TRAIT_2
#define FALLTHROUGHABLE TRAIT_3

typedef struct defer_scope {
    ast_expr_list_t list;
    struct defer_scope *parent;
    weak_cstr_t label;
    trait_t traits;
} defer_scope_t;

// ------------------ defer_scope_init ------------------
// Initializes a defer scope
void defer_scope_init(defer_scope_t *defer_scope, defer_scope_t *parent, weak_cstr_t label, trait_t traits);

// ------------------ defer_scope_free ------------------
// Frees a defer scope
void defer_scope_free(defer_scope_t *defer_scope);

// ------------------ defer_scope_total ------------------
// Returns the number of deferred statements in this scope and ancestor scopes
length_t defer_scope_total(defer_scope_t *defer_scope);

// ------------------ defer_scope_fulfill ------------------
// Fulfill deferred statements in a single scope, handing over ownership in the process
void defer_scope_fulfill(defer_scope_t *defer_scope, ast_expr_list_t *stmt_list);

// ------------------ defer_scope_fulfill_by_cloning ------------------
// Fulfill deferred statements in a single scope WITHOUT handing over ownership
// (will clone children instead)
void defer_scope_fulfill_by_cloning(defer_scope_t *defer_scope, ast_expr_list_t *stmt_list);

// ------------------ defer_scope_rewind ------------------
// Fulfill current scope's deferred statements and duplicate parent scopes' deferred statements that would
// normally be skipped over by 'break' or 'continue'
void defer_scope_rewind(defer_scope_t *defer_scope, ast_expr_list_t *stmt_list, trait_t scope_trait, weak_cstr_t label);

// ------------------ parse_stmts ------------------
// Parses one or more statements into two lists.
// 'stmt_list' for standard statements
// 'defer_scope' scope in which deferred statements will use
// NOTE: On failure, stmt_list and defer_scope->list.statements may need to be freed
#define PARSE_STMTS_STANDARD           TRAIT_NONE // Standard mode (will parse multiple statements)
#define PARSE_STMTS_SINGLE             TRAIT_1    // Single statement mode (will parse a single statement)
#define PARSE_STMTS_PARENT_DEFER_SCOPE TRAIT_2    // Parent defer scope mode (won't create a separate defer scope for the statements)
#define PARSE_STMTS_NO_JOINING         TRAIT_3    // Disable statement join operator ';'
errorcode_t parse_stmts(parse_ctx_t *ctx, ast_expr_list_t *stmt_list, defer_scope_t *defer_scope, trait_t mode);

// ------------------ parse_stmt_call ------------------
// Parses a function call statement
errorcode_t parse_stmt_call(parse_ctx_t *ctx, ast_expr_list_t *expr_list, bool is_tentative);

// ------------------ parse_stmt_declare ------------------
// Parses a variable declaration statement
errorcode_t parse_stmt_declare(parse_ctx_t *ctx, ast_expr_list_t *expr_list);

// ------------------ parse_stmt_mid_declare ------------------
// Parses the body portion of variable declaration statement
// DANGEROUS: Don't use this function unless you know what you're doing
// NOTE: Ownership of 'master_type' is taken
// NOTE: 'length' is used for the length of both 'names' and 'sources'
// NOTE: No ownership is taken of the arrays 'names' and 'sources' themselves
errorcode_t parse_stmt_mid_declare(parse_ctx_t *ctx, ast_expr_list_t *stmt_list, ast_type_t master_type, weak_cstr_t *names, source_t *source_list, length_t length, trait_t traits);

// ------------------ parse_switch ------------------
// Parses a switch statement
errorcode_t parse_switch(parse_ctx_t *ctx, ast_expr_list_t *stmt_list, defer_scope_t *parent_defer_scope, bool is_exhaustive);

// ------------------ parse_onetime_conditional ------------------
// Parses a onetime-evaluated conditional,
// such as 'if' or 'unless'
errorcode_t parse_onetime_conditional(parse_ctx_t *ctx, ast_expr_list_t *stmt_list, defer_scope_t *defer_scope);

// ------------------ parse_conditionless_block ------------------
// Parses a conditionless block
errorcode_t parse_conditionless_block(parse_ctx_t *ctx, ast_expr_list_t *stmt_list, defer_scope_t *defer_scope);

// ------------------ parse_mutable_expr_operation ------------------
// Parses a statement that begins with a mutable expression
// e.g.  variable = value     or    my_array[index].doSomething()
// NOTE: Assumes 'stmt_list' has enough space for another statement
// NOTE: expand() should be used on stmt_list to make room sometime before calling
// NOTE: Takes ownership of 'mutable_expr' and will free it in the case of failure
errorcode_t parse_mutable_expr_operation(parse_ctx_t *ctx, ast_expr_list_t *stmt_list);
errorcode_t parse_mid_mutable_expr_operation(parse_ctx_t *ctx, ast_expr_list_t *stmt_list, ast_expr_t *mutable_expr, source_t source);

// ------------------ parse_ambiguous_open_bracket ------------------
// This function is used to disambiguate between the two following syntaxes:
// variable[value] ... 
// vairable [value] Type
// And then injects the values along the way into the proper context
// Must also handle cases like:
// variable[value][value] ...
// variable [value] [value] Type
errorcode_t parse_ambiguous_open_bracket(parse_ctx_t *ctx, ast_expr_list_t *stmt_list);

// ------------------ parse_llvm_asm ------------------
// Parses an inline LLVM assembly statement
errorcode_t parse_llvm_asm(parse_ctx_t *ctx, ast_expr_list_t *stmt_list);

// ------------------ parse_local_constant_declaration ------------------
// Parses a local named constant expression declaration
// NOTE: Assumes 'stmt_list' has enough space for another statement
// NOTE: expand() should be used on stmt_list to make room sometime before calling
errorcode_t parse_local_constant_declaration(parse_ctx_t *ctx, ast_expr_list_t *stmt_list, source_t source);

// ------------------ parse_block_begin ------------------
// Parses the token that begins a block
// Usually this would be '{' or ','
// But it can also be things like words and keywords
// Stores the specified statement mode in 'out_stmts_mode' if successful
// Returns FAILURE on failure and SUCCESS on success
errorcode_t parse_block_beginning(parse_ctx_t *ctx, weak_cstr_t block_readable_mother, unsigned int *out_stmts_mode);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_PARSE_STMT_H
