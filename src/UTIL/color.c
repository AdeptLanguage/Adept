
#include <stdio.h>

#include "UTIL/color.h"

#ifdef __WIN32__
#include <windows.h>

void terminal_set_color_win32(char color){
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

void terminal_set_color_posix(char color){
    switch(color){
    case TERMINAL_COLOR_DEFAULT:
        fputs("\e[0m", stdout);
        fflush(stdout);
        break;
    case TERMINAL_COLOR_RED:
        fputs("\e[31;1m", stdout);
        break;
    case TERMINAL_COLOR_YELLOW:
        fputs("\e[33;1m", stdout);
        break;
    case TERMINAL_COLOR_WHITE:
        fputs("\e[37;1m", stdout);
        break;
    case TERMINAL_COLOR_BLUE:
        fputs("\e[34;1m", stdout);
        break;
    }
}

#endif
