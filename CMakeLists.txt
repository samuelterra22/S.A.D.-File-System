cmake_minimum_required(VERSION 3.15)
project(SAD VERSION 0.0.1 LANGUAGES C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_FILE_OFFSET_BITS=64")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall --pedantic -g")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake" ${CMAKE_MODULE_PATH})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_C_STANDARD_LIBRARIES  "-lm")

set(CMAKE_C_STANDARD 99)

find_package(FUSE REQUIRED)

include_directories(${FUSE_INCLUDE_DIR})
add_compile_definitions(CACHE_ENABLE=1)
add_compile_definitions(USING_ARDUINO=1)

add_executable(SAD main.c serial/serial.c serial/serial.h minifat/minifat.c minifat/minifat.h cache/cache.c cache/cache.h)

target_link_libraries(SAD ${FUSE_LIBRARIES})