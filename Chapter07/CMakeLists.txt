# Output to bin/Chapter07/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../bin/Chapter07)

# Collect all .cpp files
file(GLOB SOURCES "*.cpp")

# Files that require special handling (built externally, not by the default target)
set(STDEXEC_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/7x17-senders_dag.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/7x18-bridging_thread.cpp
)

# Files that need C++23 (std::expected)
set(CPP23_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/7x09-expected_pairing.cpp
)

foreach(SOURCE ${SOURCES})
    # Skip stdexec examples — they require NVIDIA stdexec and are built by hand.
    if(SOURCE IN_LIST STDEXEC_SOURCES)
        continue()
    endif()

    get_filename_component(EXEC_NAME ${SOURCE} NAME_WE)
    add_executable(${EXEC_NAME} ${SOURCE})
    target_link_libraries(${EXEC_NAME} PRIVATE Threads::Threads)

    # Force C++23 for files that use std::expected.
    if(SOURCE IN_LIST CPP23_SOURCES)
        target_compile_features(${EXEC_NAME} PRIVATE cxx_std_23)
    endif()
endforeach()
