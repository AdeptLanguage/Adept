
#ifndef _ISAAC_PARSE_DEPENDENCY_H
#define _ISAAC_PARSE_DEPENDENCY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "UTIL/ground.h"
#include "PARSE/parse_ctx.h"

// ------------------ parse_import ------------------
// Parses an 'import' statement
errorcode_t parse_import(parse_ctx_t* ctx);

// ------------------ parse_do_import ------------------
// Performs an 'import' statement
errorcode_t parse_do_import(parse_ctx_t *ctx, weak_cstr_t file, source_t source, bool allow_local);

// ------------------ parse_foreign_library ------------------
// Parses a foreign library (ex: foreign 'libcustom.a')
errorcode_t parse_foreign_library(parse_ctx_t* ctx);

// ------------------ parse_import_object ------------------
// Imports an object given the relative and absolute filenames
errorcode_t parse_import_object(parse_ctx_t *ctx, strong_cstr_t relative_filename, strong_cstr_t absolute_filename);

// ------------------ parse_find_import ------------------
// Finds the best file to use given a filename
// NOTE: Returns NULL on error
maybe_null_strong_cstr_t parse_find_import(parse_ctx_t *ctx, weak_cstr_t filename, source_t source, bool allow_local_import);

// ------------------ parse_standard_library_component ------------------
// Parses a standard library component such as "a/b/c/d" into a string
maybe_null_strong_cstr_t parse_standard_library_component(parse_ctx_t *ctx, source_t *out_source);

// ------------------ parse_resolve_import ------------------
// Attempts to create an absolute filename for a file
// NOTE: Returns NULL on error
maybe_null_strong_cstr_t parse_resolve_import(parse_ctx_t *ctx, weak_cstr_t filename);

// ------------------ already_imported ------------------
// Returns whether or not the file has already been imported
bool already_imported(parse_ctx_t *ctx, weak_cstr_t filename);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_PARSE_DEPENDENCY_H
