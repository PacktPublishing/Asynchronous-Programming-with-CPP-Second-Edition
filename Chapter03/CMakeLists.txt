# Output to bin/Chapter03/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../bin/Chapter03)

# Collect all .cpp files
file(GLOB SOURCES "*.cpp")

# Files that need the NVIDIA stdexec reference implementation. They are built
# like any other example when stdexec is available (which is the default, since
# the root CMakeLists.txt fetches it) and skipped only when it is not.
set(STDEXEC_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/3x19-cpp26_std_execution.cpp
)

foreach(SOURCE ${SOURCES})
    get_filename_component(EXEC_NAME ${SOURCE} NAME_WE)

    if(SOURCE IN_LIST STDEXEC_SOURCES)
        if(NOT STDEXEC_FOUND)
            message(STATUS
                "Chapter03: stdexec not available — skipping ${EXEC_NAME}")
            continue()
        endif()
        add_executable(${EXEC_NAME} ${SOURCE})
        target_compile_features(${EXEC_NAME} PRIVATE cxx_std_23)
        target_link_libraries(${EXEC_NAME} PRIVATE STDEXEC::stdexec)
        continue()
    endif()

    add_executable(${EXEC_NAME} ${SOURCE})
    target_link_libraries(${EXEC_NAME} PRIVATE Threads::Threads)
endforeach()

# Mirror the data/ directory next to the binaries so 3x01-concurrent_file_processor
# can find ./data regardless of where it is launched from.
add_custom_target(copy_chapter03_data ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/data
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/data
    COMMENT "Copying Chapter03/data to bin/Chapter03/data"
)
