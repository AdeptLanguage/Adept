
# How to Compile

## mingw64
- Obtain LLVM 7 headers and libraries
- Change values in beginning of Makefile (e.x. WINDOWS_CC, WINDOWS_CXX, WINDOWS_LLVM_LIB, etc.)
- `mingw32-make SHELL=cmd`
- If linker fails, change LLVM_LIBS value inside of Makefile under Windows_NT to be the result of `llvm-config --link-static --libs'

## unix
- Obtain LLVM 7 headers and libraries
- Change values in beginning of Makefile (e.x. UNIX_CC, UNIX_CXX, UNIX_LLVM_LIB, etc.)
- `make`
- If linker fails, change LLVM_LIBS value inside of Makefile under either UNIX/Darwin or UNIX/else to be the result of `llvm-config --link-static --libs'

## msvc
- Currently not supported
