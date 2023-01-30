
#include "UTIL/ground.h"
#include "UTIL/hash.h"

hash_t hash_data(const void *data, length_t size){
    hash_t hash = 0;
    for(length_t i = 0; i != size; i++){
        hash = (hash * 31) + (hash_t)((char*) data)[i];
    }
    return hash;
}

hash_t hash_string(const char *s){
    return hash_data(s, strlen(s));
}

hash_t hash_strings(char *strings[], length_t num_strings){
    if(num_strings == 0) return 0;

    hash_t hash = hash_string(strings[0]);

    for(length_t i = 1; i < num_strings; i++){
        hash = hash_combine(hash, hash_string(strings[i]));
    }

    return hash;
}

hash_t hash_combine(hash_t h1, hash_t h2){
    hash_t hash = h1;
    for(length_t i = 0; i != sizeof(h2); i++){
        hash = (hash * 31) + (hash_t)((char*) &h2)[i];
    }
    return hash;
}
