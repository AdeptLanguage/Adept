
#ifdef _WIN32
    #include <windows.h>
#endif // _WIN32

#include "DRVR/compiler.h"

int main(int argc, char **argv){
    #ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    #endif

    #ifdef TRACK_MEMORY_USAGE
    memory_init();
    #endif // TRACK_MEMORY_USAGE
    
    compiler_t compiler;
    compiler_init(&compiler);
    int exitcode = compiler_run(&compiler, argc, argv);
    compiler_free(&compiler);

    #ifdef TRACK_MEMORY_USAGE
    memory_scan();
    #endif // TRACK_MEMORY_USAGE

    return exitcode;
}
