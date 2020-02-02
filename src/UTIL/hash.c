
#include "UTIL/hash.h"

hash_t hash_data(void *data, length_t size){
    hash_t hash = 0;
    for(length_t i = 0; i != size; i++){
        hash = (hash * 31) + ((char*) data)[i];
    }
    return hash;
}

hash_t hash_combine(hash_t h1, hash_t h2){
    hash_t hash = h1;
    for(length_t i = 0; i != sizeof(h2); i++){
        hash = (hash * 31) + ((char*) &h2)[i];
    }
    return hash;
}
