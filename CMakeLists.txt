cmake_minimum_required(VERSION 2.8)
project("json-root")

set(INCLUDES "${CMAKE_SOURCE_DIR}")

include_directories(${INCLUDES})

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory("test")
add_subdirectory("json")
