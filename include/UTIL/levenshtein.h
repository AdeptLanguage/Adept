
#ifndef LEVENSHTEIN_H
#define LEVENSHTEIN_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ============================== levanshtein.h ==============================
    Module for calculating levanshtein distance
    ---------------------------------------------------------------------------
*/

int levenshtein(const char *s1, const char *s2);
int levenshtein_overlapping(const char *s1, const char *s2);

#ifdef __cplusplus
}
#endif

#endif // LEVENSHTEIN_H
