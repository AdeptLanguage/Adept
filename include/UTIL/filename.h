
#ifndef FILENAME_H
#define FILENAME_H

/*
    ================================ filename.h ===============================
    Module for manipulating filenames
    ---------------------------------------------------------------------------
*/

#include "UTIL/ground.h"

// ---------------- filename_name ----------------
// Gets the string in a filename before a slash
// (or a backslash) as a new c-string
char* filename_name(const char *filename);

// ---------------- filename_local ----------------
// Gets the string in a filename before a slash
// (or a backslash) as a constant c-string
const char* filename_name_const(const char *filename);

// ---------------- filename_local ----------------
// Creates a path to a local filename of a file
// in terms of the current filename's path
char* filename_local(const char *current_filename, const char *local_filename);

// ---------------- filename_adept_import ----------------
// Prepends the adept import directory to a filename
char* filename_adept_import(const char *filename);

// ---------------- filename_ext ----------------
// Changes the extension of a filename
char* filename_ext(const char *filename, const char *ext_without_dot);

// ---------------- filename_absolute ----------------
// Gets the absolute filename for a filename
char* filename_absolute(const char *filename);

// ---------------- filename_auto_ext ----------------
// Append the correct file extension for the given
// mode if '*out_filename' doesn't already have it
void filename_auto_ext(char **out_filename, unsigned int mode);

// Possible file auto-extension modes
#define FILENAME_AUTO_NONE       0x00
#define FILENAME_AUTO_EXECUTABLE 0x01
#define FILENAME_AUTO_PACKAGE    0x02

// ---------------- filename_append_if_missing ----------------
// Appends a string to the filename if the filename
// doesn't already end in it
void filename_append_if_missing(char **out_filename, const char *addition);

#endif // FILENAME_H
