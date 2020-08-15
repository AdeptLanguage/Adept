
#ifndef _ISAAC_HASH_H
#define _ISAAC_HASH_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ================================ hash.h ===============================
    Module for hashing generic data
    ---------------------------------------------------------------------------
*/

#include "UTIL/ground.h"

typedef length_t hash_t;

// ---------------- hash_data ----------------
// Hashes a generic block of memory
hash_t hash_data(void *data, length_t size);

// ---------------- hash_combine ----------------
// Combines two hashes into one
hash_t hash_combine(hash_t h1, hash_t h2);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_HASH_H
