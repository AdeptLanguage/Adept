
#ifndef _ISAAC_DOWNLOAD_H
#define _ISAAC_DOWNLOAD_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ================================ download.h ===============================
    Module for downloading files
    ---------------------------------------------------------------------------
*/

#include "UTIL/ground.h"

typedef struct {
    void *bytes;
    length_t length;
} download_buffer_t;

#ifdef ADEPT_ENABLE_PACKAGE_MANAGER

// ---------------- download ----------------
// Downloads a file, returns whether successful
successful_t download(weak_cstr_t url, weak_cstr_t destination, weak_cstr_t cainfo_file);

// ---------------- download_to_memory ----------------
// Downloads a file into memory, returns whether successful
// NOTE: If successful, out_memory->buffer must be freed by the caller
successful_t download_to_memory(weak_cstr_t url, download_buffer_t *out_memory, weak_cstr_t cainfo_file);

#endif // ADEPT_ENABLE_PACKAGE_MANAGER

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_DOWNLOAD_H
