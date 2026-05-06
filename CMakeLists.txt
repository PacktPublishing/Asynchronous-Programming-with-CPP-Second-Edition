cmake_minimum_required(VERSION 3.25)

project("AsyncCpp"
        VERSION 2.0.0
        DESCRIPTION "Asynchronous Programming with C++ — 2nd Edition"
        LANGUAGES CXX)

# Default to C++23; individual targets may override to C++26
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

# Optional: build C++26 std::execution / NVIDIA stdexec examples.
# When ON, stdexec is fetched once at the project level and shared across
# every chapter that defines such a target.
option(BUILD_STDEXEC_EXAMPLES
    "Build std::execution examples (fetches NVIDIA stdexec via FetchContent; needs C++26 toolchain)" OFF)

if(BUILD_STDEXEC_EXAMPLES)
    include(FetchContent)
    set(STDEXEC_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(STDEXEC_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(stdexec
        GIT_REPOSITORY https://github.com/NVIDIA/stdexec.git
        GIT_TAG        main)
    FetchContent_MakeAvailable(stdexec)
endif()

# Add chapter subdirectories
add_subdirectory(Chapter03)
