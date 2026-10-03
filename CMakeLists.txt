cmake_minimum_required(VERSION 3.14)

# TinyCamera is header-only (see src/) - the primary distribution mechanism
# is the Arduino Library Manager (library.properties), not CMake. This file
# is for the desktop build: running the library and its examples on
# Linux/macOS via the Arduino Emulator
# (https://github.com/pschatzmann/Arduino-Emulator), with frames coming
# from a synthetic test pattern or a V4L2 webcam - see
# src/TinyCameraDesktop.h. It also lets other CMake projects consume the
# library via add_subdirectory() + target_link_libraries(... TinyCamera).
project(TinyCamera LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Only build examples/tests when this is the top-level project, not when
# another project add_subdirectory()s this one just to get the target.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(TINYCAMERA_IS_TOP_LEVEL TRUE)
else()
    set(TINYCAMERA_IS_TOP_LEVEL FALSE)
endif()

option(TINYCAMERA_BUILD_EXAMPLES "Build the example sketches for the desktop" ${TINYCAMERA_IS_TOP_LEVEL})
option(TINYCAMERA_BUILD_TESTS "Build the desktop test suite (test/desktop/)" ${TINYCAMERA_IS_TOP_LEVEL})
option(TINYCAMERA_USE_TINYJPEG "Use TinyJPEG for PIXFORMAT_JPEG and TinyCameraConvertSoftware.h" ON)

include(FetchContent)

# The library headers use the Arduino API (Arduino.h, Print, Serial,
# millis(), ...), so on the desktop they need the Arduino Emulator.
if(NOT TARGET arduino_emulator)
    FetchContent_Declare(
        arduino_emulator
        GIT_REPOSITORY https://github.com/pschatzmann/Arduino-Emulator.git
        GIT_TAG        main
    )
    FetchContent_MakeAvailable(arduino_emulator)
endif()

# Optional: TinyJPEG (header-only) enables PIXFORMAT_JPEG capture on the
# desktop and TinyCameraConvertSoftware.h. To use a local checkout instead
# of fetching, pass -DFETCHCONTENT_SOURCE_DIR_TINYJPEG=<path>.
if(TINYCAMERA_USE_TINYJPEG AND NOT TARGET TinyJPEG)
    set(TINYJPEG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        TinyJPEG
        GIT_REPOSITORY https://github.com/pschatzmann/TinyJPEG.git
        GIT_TAG        main
    )
    FetchContent_MakeAvailable(TinyJPEG)
endif()

# Header-only library target.
add_library(TinyCamera INTERFACE)
add_library(TinyCamera::TinyCamera ALIAS TinyCamera)
target_include_directories(TinyCamera INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
# C++17: the headers use inline variables (e.g. TinyCameraLogger).
target_compile_features(TinyCamera INTERFACE cxx_std_17)
target_link_libraries(TinyCamera INTERFACE arduino_emulator)
if(TARGET TinyJPEG)
    target_link_libraries(TinyCamera INTERFACE TinyJPEG)
    # TinyJPEG's encoder defaults to images at most 320 pixels wide (its
    # workspace is sized for MCU RAM); on the desktop allow up to Full HD.
    # Set as a compile definition, not in a header, so every translation
    # unit sees the same TinyJPEGEncoder layout.
    target_compile_definitions(TinyCamera INTERFACE JE_MAX_WIDTH=1920)
endif()

if(TINYCAMERA_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(TINYCAMERA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test/desktop)
endif()
