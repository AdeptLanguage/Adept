
#ifndef _ISAAC_INDEX_ID_LIST_H
#define _ISAAC_INDEX_ID_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "UTIL/ground.h"
#include "UTIL/list.h"

// ---------------- index_id_list_t ----------------
typedef listof(index_id_t, ids) index_id_list_t;
#define index_id_list_append(LIST, VALUE) list_append((LIST), (VALUE), index_id_t)
#define index_id_list_free(LIST) free((LIST)->ids)

// ---------------- func_id_list_t ----------------
typedef listof(func_id_t, ids) func_id_list_t;
#define func_id_list_append(LIST, VALUE) list_append((LIST), (VALUE), func_id_t)
#define func_id_list_free(LIST) free((LIST)->ids)

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_INDEX_ID_LIST_H
