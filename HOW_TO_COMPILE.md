
# How to Compile

## Cmake (recommended)
- Obtain LLVM headers and libraries (13.0.0+ recommended) and CURL libraries/headers for your platform
- Generate your preferred build scripts (see CMakePresets.json for examples of CMake parameters)
  - e.g. `cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/llvm -DADEPT_LINK_LLVM_STATIC=Off "-DEXTRA_REQUIRED_LIBS
    S=-lldap -lpthread -lz -lcurses"` (example for building on macOS)

- Build using preferred build method
- (Optionally) Add to path
- (Optionally) Add standard library

## Adding the Adept standard library (Recommended)
In order to use the standard library, all you have to do is clone the `AdeptImport` GitHub repo into the folder than contains the `adept` binary and rename the repo folder to `import`.

- (where `build` is the folder that contains `adept` or `adept.exe` in this case)

- `cd build`
- `git clone https://github.com/AdeptLanguage/AdeptImport`
- Rename `AdeptImport` folder to `import`
  - `mv AdeptImport import` (For Unix)
  - `rename AdeptImport import` (For Windows)


## Without CMake (discouraged)

#### Makefile - mingw64 (discouraged)

- Obtain LLVM headers and libraries (13.0.0+ recommended)
- Obtain CURL headers and libraries (unless disabled package manager)
- Change values in beginning of Makefile (e.x. WINDOWS_CC, WINDOWS_CXX, WINDOWS_LLVM_LIB, etc.)
- `mingw32-make SHELL=cmd`
- If linker fails, change LLVM_LIBS value inside of Makefile under Windows_NT to be the result of `llvm-config --link-static --libs'
- Then read the 'After compilation without CMake' section of this file

#### Makefile - unix (discouraged)

- Obtain LLVM (13.0.0+ recommended) headers and libraries
- Obtain CURL headers and libraries (unless package manager is disabled)
- Change values in beginning of Makefile (e.x. UNIX_CC, UNIX_CXX, UNIX_LLVM_LIB, etc.)
- `make`
- If linker fails, change LLVM_LIBS value inside of Makefile under either UNIX/Darwin or UNIX/else to be the result of `llvm-config --link-static --libs'
- Then read the 'After compilation without CMake' section of this file

#### msvc

- Currently not supported

#### After compilation without CMake

##### Required Files for Windows when not building with CMake

In order to build executables from Adept on Windows, a few extra helper files are required. Download `Adept-Windows-From-Source-Necessities.zip` from https://github.com/IsaacShelton/Adept/releases. Then extract the archive and place the files in your `bin` folder that contains the built `adept.exe`.

##### Adding adept.config (Recommended)

When compiling from source, you might be missing an `adept.config` file. You can obtain the default copy from [https://github.com/AdeptLanguage/Config](https://github.com/AdeptLanguage/Config) and then put it in the same folder as the built `adept.exe` .

##### Final steps

- (Optionally) Add to path
- (Optionally) Add standard library (see the "Adding the Adept Standard Library" section earlier in this file)

