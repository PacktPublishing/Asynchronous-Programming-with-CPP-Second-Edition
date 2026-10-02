# Output to bin/Chapter13/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../bin/Chapter13)

# All Chapter 13 examples use C++26 std::execution (P2300) and require the
# NVIDIA stdexec reference implementation, located at the project root. They are
# built only when stdexec is available; otherwise they are skipped, exactly like
# the other stdexec examples in this repository (see the root CMakeLists.txt and
# README.md).
if(NOT STDEXEC_FOUND)
    message(STATUS
        "Chapter13: stdexec not available — skipping std::execution examples")
    return()
endif()

file(GLOB SOURCES "*.cpp")

foreach(SOURCE ${SOURCES})
    get_filename_component(EXEC_NAME ${SOURCE} NAME_WE)
    add_executable(${EXEC_NAME} ${SOURCE})
    target_compile_features(${EXEC_NAME} PRIVATE cxx_std_23)
    target_link_libraries(${EXEC_NAME} PRIVATE STDEXEC::stdexec)
endforeach()

# GPU examples (nvexec): separate .cu files that require the NVIDIA HPC SDK
# (nvc++) and a CUDA GPU. They are OFF by default and never built by CI; enable
# with -DENABLE_NVEXEC_GPU=ON, configuring with a GPU-capable compiler such as
#   cmake --preset debug -DENABLE_NVEXEC_GPU=ON -DCMAKE_CXX_COMPILER=nvc++
# See README.md for the required stdpar/CUDA flags.
if(ENABLE_NVEXEC_GPU)
    file(GLOB GPU_SOURCES "*.cu")
    foreach(SOURCE ${GPU_SOURCES})
        get_filename_component(EXEC_NAME ${SOURCE} NAME_WE)
        # nvc++ compiles .cu as C++; mark the language so CMake does not require
        # the CUDA toolchain to be enabled for these targets.
        set_source_files_properties(${SOURCE} PROPERTIES LANGUAGE CXX)
        add_executable(${EXEC_NAME} ${SOURCE})
        target_compile_features(${EXEC_NAME} PRIVATE cxx_std_23)
        target_link_libraries(${EXEC_NAME} PRIVATE STDEXEC::stdexec)
        # GPU offload flags are toolchain-specific; supply them for nvc++.
        if(CMAKE_CXX_COMPILER_ID STREQUAL "NVHPC")
            target_compile_options(${EXEC_NAME} PRIVATE -stdpar=gpu)
            target_link_options(${EXEC_NAME} PRIVATE -stdpar=gpu)
        endif()
    endforeach()
else()
    message(STATUS
        "Chapter13: ENABLE_NVEXEC_GPU=OFF — skipping GPU (.cu) examples")
endif()
