cmake_minimum_required(VERSION 3.25)

project("AsyncCpp"
        VERSION 2.0.0
        DESCRIPTION "Asynchronous Programming with C++ — 2nd Edition"
        LANGUAGES CXX)

# Default to C++23. Individual targets may override to C++26
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output all binaries to bin/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

find_package(Threads REQUIRED)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# Clang 18 ships relaxed concepts (P2493) but does not bump __cpp_concepts to
# 202002L, so libstdc++ 13/14 hides <expected> and other C++23 headers behind
# that gate. Force the macro for Clang to unblock the standard library.
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
   AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19)
    add_compile_options(-Wno-builtin-macro-redefined -U__cpp_concepts -D__cpp_concepts=202002L)
endif()

# Optional sanitizer support
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address)
endif()

option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
if(ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=thread)
endif()

# NVIDIA stdexec (P2300 reference implementation) is required for every C++26
# std::execution example in this repository.
#
# By default (FETCH_STDEXEC=ON) CMake downloads stdexec via FetchContent, pinned
# to STDEXEC_GIT_TAG below, so a plain `cmake` build compiles those chapters with
# no manual setup. To use a local checkout instead, point STDEXEC_INCLUDE_DIR at
# its include/ directory (and STDEXEC_SRC_DIR at src/, needed by the parallel-
# scheduler example). That takes priority and no download occurs:
#   cmake --preset debug -DSTDEXEC_INCLUDE_DIR=/path/to/stdexec/include \
#                        -DSTDEXEC_SRC_DIR=/path/to/stdexec/src
# To skip the std::execution examples entirely (for example, offline builds):
#   cmake --preset debug -DFETCH_STDEXEC=OFF
set(STDEXEC_INCLUDE_DIR "" CACHE PATH "Path to NVIDIA stdexec include/ directory")
set(STDEXEC_SRC_DIR "" CACHE PATH "Path to NVIDIA stdexec src/ directory")

# Single source of truth for the stdexec commit these examples were validated
# against (2026-10-26). Bump deliberately. CI derives its checkout from this.
# This commit provides the C++26 spellings the examples now use: ex::task,
# ex::counting_scope with ex::spawn/spawn_future/associate, and
# ex::get_parallel_scheduler().
set(STDEXEC_GIT_TAG "653aca18556b3c57a40dabe01f7a45d807cef650"
    CACHE STRING "NVIDIA stdexec commit to fetch when STDEXEC_INCLUDE_DIR is unset")

option(FETCH_STDEXEC
    "Download stdexec via FetchContent when STDEXEC_INCLUDE_DIR is not provided" ON)

if(NOT STDEXEC_INCLUDE_DIR AND FETCH_STDEXEC)
    include(FetchContent)
    message(STATUS
        "Fetching stdexec (${STDEXEC_GIT_TAG}) for the std::execution examples")
    # Populate the source only. We consume its include/ and src/ directories
    # directly rather than building stdexec's own CMake targets and tests.
    # Pointing SOURCE_SUBDIR at a directory that holds no CMakeLists.txt is the
    # supported way to skip that add_subdirectory() without the deprecated
    # single-argument FetchContent_Populate().
    FetchContent_Declare(stdexec
        GIT_REPOSITORY https://github.com/NVIDIA/stdexec.git
        GIT_TAG        ${STDEXEC_GIT_TAG}
        SOURCE_SUBDIR  do-not-add-stdexec-cmake)
    FetchContent_MakeAvailable(stdexec)
    set(STDEXEC_INCLUDE_DIR ${stdexec_SOURCE_DIR}/include)
    set(STDEXEC_SRC_DIR     ${stdexec_SOURCE_DIR}/src)
endif()

# One target for every chapter to link against, so a fetched stdexec and a local
# checkout look identical from a target's point of view. The includes are marked
# SYSTEM so this project's -Wall -Wextra -pedantic do not fire inside stdexec.
if(STDEXEC_INCLUDE_DIR AND EXISTS "${STDEXEC_INCLUDE_DIR}/stdexec/execution.hpp")
    set(STDEXEC_FOUND TRUE)
    add_library(stdexec_headers INTERFACE)
    target_include_directories(stdexec_headers SYSTEM INTERFACE ${STDEXEC_INCLUDE_DIR})
    target_link_libraries(stdexec_headers INTERFACE Threads::Threads)
    add_library(STDEXEC::stdexec ALIAS stdexec_headers)
else()
    set(STDEXEC_FOUND FALSE)
    message(STATUS
        "stdexec not available — skipping the std::execution examples")
endif()

# The Chapter 13 GPU examples (nvexec, .cu files) need the NVIDIA HPC SDK
# (nvc++) and a CUDA GPU, so they are off by default and never built by CI.
# Enable with -DENABLE_NVEXEC_GPU=ON and a GPU-capable compiler (see README.md).
option(ENABLE_NVEXEC_GPU
    "Build the Chapter 13 GPU (nvexec) examples. Requires nvc++/CUDA and a GPU" OFF)

# Add chapter subdirectories
add_subdirectory(Chapter03)
add_subdirectory(Chapter07)
add_subdirectory(Chapter08)
add_subdirectory(Chapter12)
add_subdirectory(Chapter13)
