# Output to bin/Chapter03/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../bin/Chapter03)

# Collect all .cpp files
file(GLOB SOURCES "*.cpp")

# Files that require BUILD_STDEXEC_EXAMPLES=ON (defined at project root)
set(STDEXEC_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/3x19-cpp26_std_execution.cpp
)

foreach(SOURCE ${SOURCES})
    if(SOURCE IN_LIST STDEXEC_SOURCES)
        continue()
    endif()

    get_filename_component(EXEC_NAME ${SOURCE} NAME_WE)
    add_executable(${EXEC_NAME} ${SOURCE})
    target_link_libraries(${EXEC_NAME} PRIVATE Threads::Threads)
endforeach()

if(BUILD_STDEXEC_EXAMPLES)
    add_executable(3x19-cpp26_std_execution
        ${CMAKE_CURRENT_SOURCE_DIR}/3x19-cpp26_std_execution.cpp)
    # CMake's known-features list doesn't tag clang-18 as cxx_std_26-capable,
    # so pass the flag directly instead of using target_compile_features.
    target_compile_options(3x19-cpp26_std_execution PRIVATE -std=c++26)
    target_link_libraries(3x19-cpp26_std_execution
        PRIVATE STDEXEC::stdexec Threads::Threads)
endif()
