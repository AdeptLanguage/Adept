
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>

#include "UTIL/color.h"

#ifdef __WIN32__
#include <windows.h>

void terminal_set_color_win32(enum color_h_terminal_color color){
    switch(color){
    case TERMINAL_COLOR_DEFAULT:
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        break;
    case TERMINAL_COLOR_RED:
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_INTENSITY);
        break;
    case TERMINAL_COLOR_YELLOW:
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        break;
    case TERMINAL_COLOR_WHITE:
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
        break;
    case TERMINAL_COLOR_BLUE:
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_BLUE | FOREGROUND_INTENSITY);
        break;
    }
}

#else

void terminal_set_color_posix(enum color_h_terminal_color color){
    switch(color){
    case TERMINAL_COLOR_DEFAULT:
        fputs("\033[0m", stdout);
        fflush(stdout);
        break;
    case TERMINAL_COLOR_RED:
        fputs("\033[31;1m", stdout);
        break;
    case TERMINAL_COLOR_YELLOW:
        fputs("\033[33;1m", stdout);
        break;
    case TERMINAL_COLOR_WHITE:
        fputs("\033[37;1m", stdout);
        break;
    case TERMINAL_COLOR_BLUE:
        fputs("\033[34;1m", stdout);
        break;
    }
}

#endif

void errorprintf(const char *format, ...){
    va_list args;
    va_start(args, format);
    redprintf("error: ");
    vprintf(format, args);
    va_end(args);
}

void warningprintf(const char *format, ...){
    va_list args;
    va_start(args, format);
    yellowprintf("warning: ");
    vprintf(format, args);
    va_end(args);
}

void internalerrorprintf(const char *format, ...){
    va_list args;
    va_start(args, format);
    redprintf("internal-error: ");
    vprintf(format, args);
    va_end(args);
}

void internalwarningprintf(const char *format, ...){
    va_list args;
    va_start(args, format);
    yellowprintf("internal-warning: ");
    vprintf(format, args);
    va_end(args);
}

noreturn void die(const char *format, ...){
    va_list args;
    va_start(args, format);
    redprintf("critical-error: ");
    vprintf(format, args);
    va_end(args);
    redprintf("    Exiting with status of -1\n");

    #if ENABLE_DEBUG_FEATURES
        abort();
    #else
        exit(-1);
    #endif
}
