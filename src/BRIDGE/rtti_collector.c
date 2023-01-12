
#include "BRIDGE/rtti_collector.h"

void rtti_collector_init(rtti_collector_t *collector){
    ast_type_set_init(&collector->ast_types_used, 2048);
}

void rtti_collector_free(rtti_collector_t *collector){
    ast_type_set_free(&collector->ast_types_used);
}

void rtti_collector_mention(rtti_collector_t *collector, const ast_type_t *type){
    ast_type_set_insert(&collector->ast_types_used, type);
}

bool rtti_collector_mention_base(rtti_collector_t *collector, const char *name){
    ast_type_t type = ast_type_make_base(strclone(name));
    bool inserted = ast_type_set_insert(&collector->ast_types_used, &type);
    ast_type_free(&type);
    return inserted;
}
