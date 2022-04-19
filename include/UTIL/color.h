
#ifndef _ISAAC_COLOR_H
#define _ISAAC_COLOR_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ================================= color.h =================================
    Module for printing out colored text to the terminal
    ---------------------------------------------------------------------------
*/

#include <stdio.h> // IWYU pragma: keep
#include <stdnoreturn.h>

// Possible colors for terminal_set_color
enum color_h_terminal_color {
    TERMINAL_COLOR_DEFAULT,
    TERMINAL_COLOR_RED,
    TERMINAL_COLOR_YELLOW,
    TERMINAL_COLOR_WHITE,
    TERMINAL_COLOR_BLUE,
    TERMINAL_COLOR_LIGHT_BLUE,
};

// ---------------- terminal_set_color(color) ----------------
// Sets the output color for the current terminal
#ifdef _WIN32
#define terminal_set_color(a) terminal_set_color_win32(a)
void terminal_set_color_win32(enum color_h_terminal_color color);
#else
#define terminal_set_color(a) terminal_set_color_posix(a)
void terminal_set_color_posix(enum color_h_terminal_color color);
#endif

#if defined(ADEPT_INSIGHT_BUILD) && !defined(__EMSCRIPTEN__)
#define redprintf(...) ((void) 0)
#define yellowprintf(...) ((void) 0)
#define whiteprintf(...) ((void) 0)
#define blueprintf(...) ((void) 0)
#else
// ---------------- redprintf(...) ----------------
// Temporarily sets to red for a call to printf
#define redprintf(...) { \
    terminal_set_color(TERMINAL_COLOR_RED); \
    printf(__VA_ARGS__); \
    terminal_set_color(TERMINAL_COLOR_DEFAULT); \
}

// ---------------- yellowprintf(...) ----------------
// Temporarily sets to yellow for a call to printf
#define yellowprintf(...) { \
    terminal_set_color(TERMINAL_COLOR_YELLOW); \
    printf(__VA_ARGS__); \
    terminal_set_color(TERMINAL_COLOR_DEFAULT); \
}

// ---------------- whiteprintf(...) ----------------
// Temporarily sets to white for a call to printf
#define whiteprintf(...) { \
    terminal_set_color(TERMINAL_COLOR_WHITE); \
    printf(__VA_ARGS__); \
    terminal_set_color(TERMINAL_COLOR_DEFAULT); \
}

// ---------------- blueprintf(...) ----------------
// Temporarily sets to blue for a call to printf
#define blueprintf(...) { \
    terminal_set_color(TERMINAL_COLOR_BLUE); \
    printf(__VA_ARGS__); \
    terminal_set_color(TERMINAL_COLOR_DEFAULT); \
}
#endif

// ---------------- errorprintf  ----------------
// Version of 'printf' for non-specific errors
void errorprintf(const char *format, ...);

// ---------------- warningprintf ----------------
// Version of 'printf' for non-specific warnings
void warningprintf(const char *format, ...);

// ---------------- internalerrorprintf  ----------------
// Version of 'printf' for non-specific internal errors
void internalerrorprintf(const char *format, ...);

// ---------------- warningprintf ----------------
// Version of 'printf' for non-specific internal warnings
void internalwarningprintf(const char *format, ...);

// ---------------- die ----------------
// Displays a critical error and then calls exit(-1)
noreturn void die(const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_COLOR_H
