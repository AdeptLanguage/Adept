
#ifndef SEARCH_H
#define SEARCH_H

/*
    ================================= search.h =================================
    Module for performing special search algorithms
    ----------------------------------------------------------------------------
*/

#include "UTIL/ground.h"

// ---------------- binary_string_search ----------------
// Performs a binary search to find a string in an array
int binary_string_search(const char * const strings[], length_t string_count, const char *target);

#endif // SEARCH_H
