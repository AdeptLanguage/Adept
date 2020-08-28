
# How to Compile

## mingw64
- Obtain LLVM 7 headers and libraries
- Change values in beginning of Makefile (e.x. WINDOWS_CC, WINDOWS_CXX, WINDOWS_LLVM_LIB, etc.)
- If experimental package manager is desired, obtain libcurl headers and libraries
- Set whether ENABLE_ADEPT_PACKAGE_MANAGER in Makefile (if 'true' then libcurl is required)
- `mingw32-make SHELL=cmd`
- If linker fails, change LLVM_LIBS value inside of Makefile under Windows_NT to be the result of `llvm-config --link-static --libs'

## unix
- Obtain LLVM 10 headers and libraries
- Change values in beginning of Makefile (e.x. UNIX_CC, UNIX_CXX, UNIX_LLVM_LIB, etc.)
- If experimental package manager is desired, obtain libcurl headers and libraries
- Set whether ENABLE_ADEPT_PACKAGE_MANAGER in Makefile (if 'true' then libcurl is required)
- `make`
- If linker fails, change LLVM_LIBS value inside of Makefile under either UNIX/Darwin or UNIX/else to be the result of `llvm-config --link-static --libs'

## msvc
- Currently not supported


# After compilation

## Adding the Adept standard library
In order to use the standard library, all you have to do is clone the AdeptImport GitHub repo into the bin folder and rename it
- `cd bin`
- `git clone https://github.com/IsaacShelton/AdeptImport`
- `mv AdeptImport import` (For Unix)
- `rename AdeptImport import` (For Windows)
