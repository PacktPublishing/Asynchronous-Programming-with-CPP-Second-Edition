# Output to bin/Chapter12/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../bin/Chapter12)

# All Chapter 12 examples use C++26 std::execution (P2300) and require the
# NVIDIA stdexec reference implementation, located at the project root. They are
# built only when stdexec is available. Otherwise they are skipped, exactly like
# the other stdexec examples in this repository (see the root CMakeLists.txt and
# README.md).
if(NOT STDEXEC_FOUND)
    message(STATUS
        "Chapter12: stdexec not available. Skipping std::execution examples")
    return()
endif()

file(GLOB SOURCES "*.cpp")

# 12x04 additionally needs the parallel_scheduler default backend translation
# unit, provided by STDEXEC_SRC_DIR/parallel_scheduler/parallel_scheduler.cpp.
set(PARALLEL_SCHED_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/12x04-parallel_scheduler.cpp)

foreach(SOURCE ${SOURCES})
    if(SOURCE STREQUAL PARALLEL_SCHED_SRC)
        continue()  # handled separately below
    endif()

    get_filename_component(EXEC_NAME ${SOURCE} NAME_WE)
    add_executable(${EXEC_NAME} ${SOURCE})
    target_compile_features(${EXEC_NAME} PRIVATE cxx_std_23)
    target_link_libraries(${EXEC_NAME} PRIVATE STDEXEC::stdexec)
endforeach()

# The parallel-scheduler example, only if its backend source is available.
#
# That backend must be compiled straight into the executable rather than linked
# as a static library: it registers itself with a static initializer instead of
# through a referenced symbol, so a library link lets the linker drop the object
# file entirely and get_parallel_scheduler() then faults at run time.
set(PARALLEL_BACKEND
    "${STDEXEC_SRC_DIR}/parallel_scheduler/parallel_scheduler.cpp")
if(STDEXEC_SRC_DIR AND EXISTS "${PARALLEL_BACKEND}")
    add_executable(12x04-parallel_scheduler
        ${PARALLEL_SCHED_SRC} ${PARALLEL_BACKEND})
    target_compile_features(12x04-parallel_scheduler PRIVATE cxx_std_23)
    target_link_libraries(12x04-parallel_scheduler PRIVATE STDEXEC::stdexec)
else()
    message(STATUS
        "Chapter12: STDEXEC_SRC_DIR not set — "
        "skipping 12x04-parallel_scheduler (needs the backend TU)")
endif()
