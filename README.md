# armajitto

ARMv4T/v5TE to x86-64 dynamic recompiler.

**Note: this is alpha software. Expect bugs, crashes and weird behavior.**

## Building

This library requires [CMake](https://cmake.org/) 3.19 or later to build.

armajitto has been succesfully compiled on the following toolchains:

- Windows 10 (10.0.19045.2486)
  - Clang 15.0.1 included with Visual Studio 2022
  - GCC 12.2.0 from MSYS2 MinGW 64-bit (MINGW64_NT-10.0-19045 version 3.3.6-341.x86_64)
- Linux (Ubuntu 20.04.5 LTS)
  - Clang 11.0.0
  - GCC 11.1.0
