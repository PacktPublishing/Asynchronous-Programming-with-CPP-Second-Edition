# Output to bin/Chapter07/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../bin/Chapter07)

# Collect all .cpp files
file(GLOB SOURCES "*.cpp")

# Files that need the NVIDIA stdexec reference implementation. They are built
# like any other example when stdexec is available (which is the default, since
# the root CMakeLists.txt fetches it) and skipped only when it is not.
set(STDEXEC_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/7x17-senders_dag.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/7x18-bridging_thread.cpp
)

# Files that need C++23 (std::expected)
set(CPP23_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/7x09-expected_pairing.cpp
)

foreach(SOURCE ${SOURCES})
    get_filename_component(EXEC_NAME ${SOURCE} NAME_WE)

    if(SOURCE IN_LIST STDEXEC_SOURCES)
        if(NOT STDEXEC_FOUND)
            message(STATUS
                "Chapter07: stdexec not available — skipping ${EXEC_NAME}")
            continue()
        endif()
        add_executable(${EXEC_NAME} ${SOURCE})
        target_compile_features(${EXEC_NAME} PRIVATE cxx_std_23)
        target_link_libraries(${EXEC_NAME} PRIVATE STDEXEC::stdexec)
        continue()
    endif()

    add_executable(${EXEC_NAME} ${SOURCE})
    target_link_libraries(${EXEC_NAME} PRIVATE Threads::Threads)

    # Force C++23 for files that use std::expected.
    if(SOURCE IN_LIST CPP23_SOURCES)
        target_compile_features(${EXEC_NAME} PRIVATE cxx_std_23)
    endif()
endforeach()
