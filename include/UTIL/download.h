
#ifndef _ISAAC_DOWNLOAD_H
#define _ISAAC_DOWNLOAD_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ================================ filename.h ===============================
    Module for manipulating filenames
    ---------------------------------------------------------------------------
*/

#include "UTIL/ground.h"

typedef struct {
    void *bytes;
    length_t length;

    #ifdef TRACK_MEMORY_USAGE
    length_t capacity;
    #endif // TRACK_MEMORY_USAGE
} download_buffer_t;

#ifdef ADEPT_ENABLE_PACKAGE_MANAGER

// ---------------- download ----------------
// Downloads a file, returns whether successful
successful_t download(weak_cstr_t url, weak_cstr_t destination);

// ---------------- filename_without_ext ----------------
// Downloads a file into memory, returns whether successful
// NOTE: If successful, out_memory->buffer must be freed by the caller
successful_t download_to_memory(weak_cstr_t url, download_buffer_t *out_memory);

#endif // ADEPT_ENABLE_PACKAGE_MANAGER

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_DOWNLOAD_H
