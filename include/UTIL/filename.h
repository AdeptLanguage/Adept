
#ifndef _ISAAC_FILENAME_H
#define _ISAAC_FILENAME_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ================================ filename.h ===============================
    Module for manipulating filenames
    ---------------------------------------------------------------------------
*/

#include "UTIL/ground.h"

// ---------------- filename_name ----------------
// Gets the string in a filename after a slash
// (or a backslash) as a new c-string
strong_cstr_t filename_name(const char *filename);

// ---------------- filename_name_const ----------------
// Gets the string in a filename after a slash
// (or a backslash) as a weak c-string
weak_cstr_t filename_name_const(weak_cstr_t filename);

// ---------------- filename_path ----------------
// Gets the path in a filename after as a new c-string
// (includes slash)
strong_cstr_t filename_path(const char *filename);

// ---------------- filename_local ----------------
// Creates a path to a local filename of a file
// in terms of the current filename's path
strong_cstr_t filename_local(const char *current_filename, const char *local_filename);

// ---------------- filename_adept_import ----------------
// Prepends the adept import directory to a filename
strong_cstr_t filename_adept_import(const char *root, const char *filename);

// ---------------- filename_ext ----------------
// Changes the extension of a filename
strong_cstr_t filename_ext(const char *filename, const char *ext_without_dot);

// ---------------- filename_absolute ----------------
// Gets the absolute filename for a filename
strong_cstr_t filename_absolute(const char *filename);

// ---------------- filename_auto_ext ----------------
// Append the correct file extension for the given
// mode if '*out_filename' doesn't already have it
void filename_auto_ext(strong_cstr_t *out_filename, unsigned int mode);

// Possible file auto-extension modes
#define FILENAME_AUTO_NONE       0x00
#define FILENAME_AUTO_EXECUTABLE 0x01
#define FILENAME_AUTO_PACKAGE    0x02

// ---------------- filename_append_if_missing ----------------
// Appends a string to the filename if the filename
// doesn't already end in it
void filename_append_if_missing(strong_cstr_t *out_filename, const char *addition);

// ---------------- filename_without_ext ----------------
// Returns the filename without the extension
char *filename_without_ext(char *filename);

// ---------------- filename_prepend_dotslash_if_needed ----------------
void filename_prepend_dotslash_if_needed(strong_cstr_t *filename);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_FILENAME_H
