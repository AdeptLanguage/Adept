
# How to Compile

## mingw64
- Obtain LLVM 7 headers and libraries
- Change values in beginning of Makefile (e.x. WINDOWS_CC, WINDOWS_CXX, WINDOWS_LLVM_LIB, etc.)
- If experimental package manager is desired, obtain libcurl headers and libraries
- Set whether ENABLE_ADEPT_PACKAGE_MANAGER in Makefile (if 'true' then libcurl is required)
- `mingw32-make SHELL=cmd`
- If linker fails, change LLVM_LIBS value inside of Makefile under Windows_NT to be the result of `llvm-config --link-static --libs'
- Then read the 'After compilation' section of this file

## unix
- Obtain LLVM 10 headers and libraries
- Change values in beginning of Makefile (e.x. UNIX_CC, UNIX_CXX, UNIX_LLVM_LIB, etc.)
- If experimental package manager is desired, obtain libcurl headers and libraries
- Set whether ENABLE_ADEPT_PACKAGE_MANAGER in Makefile (if 'true' then libcurl is required)
- `make`
- If linker fails, change LLVM_LIBS value inside of Makefile under either UNIX/Darwin or UNIX/else to be the result of `llvm-config --link-static --libs'
- Then read the 'After compilation' section of this file

## msvc
- Currently not supported


# After compilation

## Required Files for Windows (Required on Windows)
In order to build executables from Adept on Windows, a few extra helper files are required. Download `Adept-Windows-From-Source-Necessities.zip` from https://github.com/IsaacShelton/Adept/releases. Then extract the archive and place the files in your `bin` folder that contains the built `adept.exe`.

## Adding the Adept standard library (Recommended)
In order to use the standard library, all you have to do is clone the AdeptImport GitHub repo into the bin folder and rename it
- `cd bin`
- `git clone https://github.com/IsaacShelton/AdeptImport`
- `mv AdeptImport import` (For Unix)
- `rename AdeptImport import` (For Windows)
