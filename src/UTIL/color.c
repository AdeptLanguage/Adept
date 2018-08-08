
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
    }
}

#endif // __WIN32__

// If not windows, then terminal color will not be changed
