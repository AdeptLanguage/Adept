
#ifndef IMPROVED_QSORT_H
#define IMPROVED_QSORT_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------- improved_qsort ----------------
// Quick sort with mutable references and user-data
void improved_qsort(void *array, size_t length, size_t unit_size, int (*cmp_func)(void *user, void *a, void *b), void *user_data);

// ---------------- improved_qsort_partition ----------------
// Partition function for imporoved_qsort
size_t improved_qsort_partition(void *array, size_t beginning, size_t end, size_t unit_size, int (*cmp_func)(void *user, void *a, void *b), void *user_data);

#ifdef __cplusplus
}
#endif

#endif // IMPROVED_QSORT_H
