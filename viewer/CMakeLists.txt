cmake_minimum_required(VERSION 3.18)
project(sdviewer VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2>=2.0.18)
pkg_check_modules(AV REQUIRED IMPORTED_TARGET libavcodec libavutil)
pkg_check_modules(TURBOJPEG REQUIRED IMPORTED_TARGET libturbojpeg)

add_executable(sdviewer
  src/main.c
  src/protocol.c
  src/decoder.c
  src/keymap.c
  src/net.c
)
target_compile_options(sdviewer PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
target_link_libraries(sdviewer PRIVATE PkgConfig::SDL2 PkgConfig::AV PkgConfig::TURBOJPEG)

install(TARGETS sdviewer RUNTIME DESTINATION bin)
