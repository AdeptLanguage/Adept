
# Windows

### MSYS2 Setup:

- Download MSYS2
- Install MSYS2
- Open the MSYS2 MINGW64 Prompt

##### MSYS2 Mingw64 Command Prompt
- `pacman -S git --noconfirm`
- `pacman -S mingw-w64-x86_64-cmake --noconfirm`
- `pacman -S mingw-w64-x86_64-ninja --noconfirm`
- `pacman -S mingw-w64-x86_64-gcc --noconfirm`
- `pacman -S mingw-w64-x86_64-llvm --noconfirm`
- `pacman -S mingw-w64-x86_64-zlib --noconfirm`
- `git clone https://github.com/AdeptLanguage/Adept`
- `cd Adept`
- `mkdir build`
- `/mingw64/bin/cmake -B build -DCMAKE_BUILD_TYPE="Release" -DCMAKE_C_FLAGS="-w" -G Ninja`
- `/mingw64/bin/cmake --build build`
- `build/adept tests/e2e/src/hello_world/main.adept -e`

# macOS / Linux

Install build tools:

- CMake
- Ninja

Install required dependencies:

- `llvm` (preferably LLVM 17 or 18)
- `zstd` (for LLVM)
- `zlib` (for LLVM)
- `libcurl-dev`

Clone and build:

- `git clone https://github.com/AdeptLanguage/Adept`
- `cd Adept`
- `mkdir build`
- `cmake -B build -DCMAKE_BUILD_TYPE="Release" -DCMAKE_C_FLAGS="-w" -G Ninja`
- `cmake --build build`
- `build/adept tests/e2e/src/hello_world/main.adept -e`

(May not be needed) - Depending on your setup, you may need to tell CMake where to find each library. This is done by setting environment variables.

(it doesn't have to be global like in the example)

`
LLVM_DIR=/opt/homebrew/opt/llvm zstd_DIR=/usr/local/opt/zstd cmake -B build -DCMAKE_BUILD_TYPE="Release" -DCMAKE_C_FLAGS="-w" -G Ninja
`

for example.

# Linking LLVM Statically

If you wish to link LLVM statically like the precompiled binaries do, you can specify `-DADEPT_LINK_LLVM_STATIC=On` when configuring with CMake.

For example,

- `git clone https://github.com/AdeptLanguage/Adept`
- `cd Adept`
- `mkdir build`
- `cmake -B build -DCMAKE_BUILD_TYPE="Release" -DADEPT_LINK_LLVM_STATIC=On -DCMAKE_C_FLAGS="-w" -G Ninja`
- `cmake --build build`
