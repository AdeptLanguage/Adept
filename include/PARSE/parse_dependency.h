
#ifndef PARSE_DEPENDENCY_H
#define PARSE_DEPENDENCY_H

#include "UTIL/ground.h"
#include "PARSE/parse_ctx.h"

// ------------------ parse_import ------------------
// Parses an 'import' statement
int parse_import(parse_ctx_t* ctx);

// ------------------ parse_foreign_library ------------------
// Parses a foreign library (ex: foreign 'libcustom.a')
void parse_foreign_library(parse_ctx_t* ctx);

// ------------------ parse_import_object ------------------
// Imports an object given the relative and absolute filenames
int parse_import_object(parse_ctx_t *ctx, char *relative_filename, char *absolute_filename);

// ------------------ parse_find_import ------------------
// Finds the best file to use given a filename
// NOTE: Returns NULL on error
char* parse_find_import(parse_ctx_t *ctx, char *file);

// ------------------ parse_resolve_import ------------------
// Attempts to create an absolute filename for a file
// NOTE: Returns NULL on error
char* parse_resolve_import(parse_ctx_t *ctx, char *file);

// ------------------ already_imported ------------------
// Returns whether or not the file has already been imported
bool already_imported(parse_ctx_t *ctx, char *file);

#endif // PARSE_DEPENDENCY_H
