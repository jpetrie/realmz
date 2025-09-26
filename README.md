# Realmz

Realmz is a classic, turn-based RPG, originally developed for early Macintosh computers. It was originally released as shareware, with additional scenarios available for purchase. Tim has graciously agreed to a release of the original code under a non-commercial license (see "License" section below).

# License

<p xmlns:cc="http://creativecommons.org/ns#">Realmz, copyright © 1994 by Tim Phillips. Modified for compatibility with modern systems (see CHANGELOG for detailed modification notes). Realmz and its associated software, in both source code and binary formats, its game assets, and its documentation (the Licensed Material), are distributed under the terms of the <a href="https://creativecommons.org/licenses/by-nc-sa/4.0/?ref=chooser-v1" target="_blank" rel="license noopener noreferrer" style="display:inline-block;">Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International<img style="height:22px!important;margin-left:3px;vertical-align:text-bottom;" src="https://mirrors.creativecommons.org/presskit/icons/cc.svg?ref=chooser-v1" alt=""><img style="height:22px!important;margin-left:3px;vertical-align:text-bottom;" src="https://mirrors.creativecommons.org/presskit/icons/by.svg?ref=chooser-v1" alt=""><img style="height:22px!important;margin-left:3px;vertical-align:text-bottom;" src="https://mirrors.creativecommons.org/presskit/icons/nc.svg?ref=chooser-v1" alt=""><img style="height:22px!important;margin-left:3px;vertical-align:text-bottom;" src="https://mirrors.creativecommons.org/presskit/icons/sa.svg?ref=chooser-v1" alt=""></a>. The Licensed Material is provided on an as-is basis, with no warranties of any kind.</p>

# Cross-compiling for Windows from Mac

- Install [llvm-mingw](https://github.com/mstorsjo/llvm-mingw)
  - Download latest llvm-mingw-<date>-ucrt-macos-universal.tar.xz
  - Extract
  - `sudo mv ~/Downloads/llvm-mingw-<date>-ucrt-macos-universal.tar.xz /opt/llvm-mingw`
- Create a [toolchain file](https://cmake.org/cmake/help/book/mastering-cmake/chapter/Cross%20Compiling%20With%20CMake.html#toolchain-files)
- Create a target install directory for cross-compiled dependencies `mkdir ~/mingw-install`
- Cross-compile dependencies
  - SDL3 `cmake --fresh -B build_win -D CMAKE_TOOLCHAIN_FILE=~/workspace/TC-mingw.cmake -D CMAKE_INSTALL_PREFIX=~/mingw-install && cmake --build build_win --target install`
  - SDL3_ttf
    - Download Freetype as vendored library source `./external/download.sh`
    - Configure to compile and use vendored Freetype `cmake --fresh -B build_win -D CMAKE_TOOLCHAIN_FILE=~/workspace/TC-mingw.cmake -D CMAKE_INSTALL_PREFIX=~/mingw-install -D SDLTTF_VENDORED=ON`
    - Install to mingw-install dir `cmake --build build_win --target install`
  - SDL3_image `cmake --fresh -B build_win -D CMAKE_TOOLCHAIN_FILE=~/workspace/TC-mingw.cmake -D CMAKE_INSTALL_PREFIX=~/mingw-install && cmake --build build_win --target install`
  - [zlib](https://github.com/madler/zlib) `cmake -S . -B build_win -D CMAKE_BUILD_TYPE=RelWithDebInfo -D CMAKE_TOOLCHAIN_FILE=~/workspace/TC-mingw.cmake -D CMAKE_INSTALL_PREFIX=~/mingw-install && cmake --build build_win --target install`
