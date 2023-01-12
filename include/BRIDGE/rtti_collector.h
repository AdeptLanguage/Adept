
#ifndef _ISAAC_RTTI_COLLECTOR_H
#define _ISAAC_RTTI_COLLECTOR_H

#include "AST/TYPE/ast_type_set.h"
#include "AST/TYPE/ast_type_make.h"
#include "UTIL/ground.h"
#include "UTIL/string.h"

// ---------------- rtti_collector_t ----------------
// Keeps track of what AST types have been mentioned to it
typedef struct {
    ast_type_set_t ast_types_used;
} rtti_collector_t;

void rtti_collector_init(rtti_collector_t *collector);
void rtti_collector_free(rtti_collector_t *collector);

// ---------------- rtti_collector_mention ----------------
// Mentions an AST type to an RTTI collector
void rtti_collector_mention(rtti_collector_t *collector, const ast_type_t *type);

// ---------------- rtti_collector_mention_base ----------------
// Helper to mention a simple base AST type to an RTTI collector.
// Used for mentioning built-in types
bool rtti_collector_mention_base(rtti_collector_t *collector, const char *name);

#endif // _ISAAC_RTTI_COLLECTOR_H
