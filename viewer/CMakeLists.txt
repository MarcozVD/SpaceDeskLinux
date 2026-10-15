cmake_minimum_required(VERSION 3.18)
project(sdviewer VERSION 0.2.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(SDV_STATIC_TURBOJPEG "Link TurboJPEG statically (target lacks libturbojpeg0)" OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2>=2.0.18)
pkg_check_modules(AV REQUIRED IMPORTED_TARGET libavcodec libavutil libva)
pkg_check_modules(TURBOJPEG REQUIRED IMPORTED_TARGET libturbojpeg)
if(SDV_STATIC_TURBOJPEG)
  find_library(TURBOJPEG_STATIC_LIB NAMES libturbojpeg.a REQUIRED)
endif()

add_executable(sdviewer
  src/main.c
  src/ui.c
  src/session.c
  src/audio.c
  src/discovery.c
  src/settings.c
  src/protocol.c
  src/decoder.c
  src/keymap.c
  src/net.c
  src/nk_impl.c
)
target_include_directories(sdviewer PRIVATE src third_party/nuklear)
target_compile_options(sdviewer PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
# vendored Nuklear: not our warnings to fix
set_source_files_properties(src/nk_impl.c PROPERTIES COMPILE_OPTIONS "-w")
target_link_libraries(sdviewer PRIVATE PkgConfig::SDL2 PkgConfig::AV m)
if(SDV_STATIC_TURBOJPEG)
  target_include_directories(sdviewer PRIVATE ${TURBOJPEG_INCLUDE_DIRS})
  target_link_libraries(sdviewer PRIVATE ${TURBOJPEG_STATIC_LIB})
else()
  target_link_libraries(sdviewer PRIVATE PkgConfig::TURBOJPEG)
endif()

install(TARGETS sdviewer RUNTIME DESTINATION bin)
