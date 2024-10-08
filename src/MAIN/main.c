
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif // _WIN32

#include "DRVR/compiler.h"

int main(int argc, char **argv){
    #ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    #endif

    compiler_t compiler;
    compiler_init(&compiler);
    int exitcode = compiler_run(&compiler, argc, argv);
    compiler_free(&compiler);

    return exitcode;
}
