
#ifndef COLOR_H
#define COLOR_H

/*
    ================================= color.h =================================
    Module for printing out colored text to the terminal
    ---------------------------------------------------------------------------
*/

// Possible colors for terminal_set_color
#define TERMINAL_COLOR_DEFAULT 0x00
#define TERMINAL_COLOR_RED     0x01
#define TERMINAL_COLOR_YELLOW  0x02
#define TERMINAL_COLOR_WHITE   0x03

// ---------------- terminal_set_color(color) ----------------
// Sets the output color for the current terminal
#ifdef _WIN32
#define terminal_set_color(a) terminal_set_color_win32(a)
void terminal_set_color_win32(char color);
#else
#define terminal_set_color(a) terminal_set_color_posix(a)
void terminal_set_color_posix(char color);
#endif

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

#endif // COLOR_H
