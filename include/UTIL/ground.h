
#ifndef GROUND_H
#define GROUND_H

/*
    ================================= ground.h ================================
    Module containing basic #includes and type definitions
    ----------------------------------------------------------------------------
*/


#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include "UTIL/memory.h"

// ---------------- errorcode_t ----------------
#define SUCCESS 0
#define FAILURE 1
typedef int errorcode_t;

// ---------------- successful_t ----------------
#define SUCCESSFUL   true
#define UNSUCCESSFUL false
typedef bool successful_t;

// ---------------- strong_cstr_t ----------------
typedef char *strong_cstr_t;            // Owned C-String
typedef char *maybe_null_strong_cstr_t; // Maybe NULL Variant

// ---------------- weak_cstr_t ----------------
typedef char *weak_cstr_t;            // Unowned C-String
typedef char *maybe_null_weak_cstr_t; // Maybe NULL Variant

//

// ---------------- length_t ----------------
typedef size_t length_t;

// ---------------- source_t ----------------
typedef struct {
    length_t index;
    length_t object_index;
} source_t;

// ---------------- source_t ----------------
typedef long long maybe_index_t;

#endif // GROUND_H
