
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

typedef size_t length_t;

typedef struct {
    length_t index;
    length_t object_index;
} source_t;

#endif // GROUND_H
