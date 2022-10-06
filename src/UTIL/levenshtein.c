
// Levenshtein's distance algorithm
// Based on: https://en.wikibooks.org/wiki/Algorithm_Implementation/Strings/Levenshtein_distance

#include <string.h>

#include "UTIL/levenshtein.h"

static inline unsigned int min2(unsigned int a, unsigned int b){
    return a < b ? a : b;
}

static inline unsigned int min3(unsigned int a, unsigned int b, unsigned int c){
    return min2(min2(a, b), c);
}

static inline int levenshtein_impl(const char *s1, const char *s2, unsigned int s1len, unsigned int s2len){
    unsigned int x, y, matrix[s2len+1][s1len+1];

    matrix[0][0] = 0;
    for (x = 1; x <= s2len; x++)
        matrix[x][0] = matrix[x-1][0] + 1;
    for (y = 1; y <= s1len; y++)
        matrix[0][y] = matrix[0][y-1] + 1;
    for (x = 1; x <= s2len; x++)
        for (y = 1; y <= s1len; y++)
            matrix[x][y] = min3(matrix[x-1][y] + 1, matrix[x][y-1] + 1, matrix[x-1][y-1] + (s1[y-1] == s2[x-1] ? 0 : 1));
    
    return matrix[s2len][s1len];
}

int levenshtein(const char *s1, const char *s2) {
    return levenshtein_impl(s1, s2, strlen(s1), strlen(s2));
}

int levenshtein_overlapping(const char *s1, const char *s2){
    unsigned int overlap = min2(strlen(s1), strlen(s2));
    return levenshtein_impl(s1, s2, overlap, overlap);
}
