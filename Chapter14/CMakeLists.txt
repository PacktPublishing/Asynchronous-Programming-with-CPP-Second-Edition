# Output to bin/Chapter14/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../bin/Chapter14)

# Every Chapter 14 example needs Boost.Asio (and the HTTP examples Boost.Beast),
# both header-only. Boost 1.90 is the floor, because 14x10-http_v3.cpp uses the
# result-returning post() added in that release; the rest of the chapter builds
# against 1.86, and everything was tested with 1.92. The lookup happens in the
# root CMakeLists.txt; if Boost was not found, skip.
if(NOT BOOST_FOUND)
    message(STATUS "Chapter14: Boost >= 1.90 not available — skipping examples")
    return()
endif()

file(GLOB SOURCES "*.cpp")

# The std::execution bridge examples additionally need NVIDIA stdexec and are
# built only when it is available, like Chapters 12 and 13.
set(STDEXEC_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/14x14-asio_scheduler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/14x15-use_sender.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/14x16-sender_pipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/14x17-asio_in_task.cpp
)

foreach(SOURCE ${SOURCES})
    get_filename_component(EXEC_NAME ${SOURCE} NAME_WE)
    if(SOURCE IN_LIST STDEXEC_SOURCES)
        if(NOT STDEXEC_FOUND)
            message(STATUS "Chapter14: stdexec not available — skipping ${EXEC_NAME}")
            continue()
        endif()
        add_executable(${EXEC_NAME} ${SOURCE})
        target_link_libraries(${EXEC_NAME} PRIVATE Boost::headers STDEXEC::stdexec)
    else()
        add_executable(${EXEC_NAME} ${SOURCE})
        target_link_libraries(${EXEC_NAME} PRIVATE Boost::headers Threads::Threads)
    endif()
    target_compile_features(${EXEC_NAME} PRIVATE cxx_std_23)
endforeach()
