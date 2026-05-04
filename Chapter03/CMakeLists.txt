# Output to bin/Chapter03/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../bin/Chapter03)

# Collect all .cpp files
file(GLOB SOURCES "*.cpp")

# Files that require special handling
set(STDEXEC_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/3x19-cpp26_std_execution.cpp
)

foreach(SOURCE ${SOURCES})
    # Skip stdexec example (requires NVIDIA stdexec, built separately)
    if(SOURCE IN_LIST STDEXEC_SOURCES)
        continue()
    endif()

    get_filename_component(EXEC_NAME ${SOURCE} NAME_WE)
    add_executable(${EXEC_NAME} ${SOURCE})
    target_link_libraries(${EXEC_NAME} PRIVATE Threads::Threads)
endforeach()
