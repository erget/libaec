cmake_minimum_required(VERSION 2.6)
include(CheckIncludeFiles)
include(TestBigEndian)
project(libaec)
set(libaec_VERSION_MAJOR 0)
set(libaec_VERSION_MINOR 2)
set(CMAKE_BUILD_TYPE Release)
enable_testing()

check_include_files(malloc.h HAVE_MALLOC_H)
check_include_files(stdint.h HAVE_STDINT_H)
test_big_endian(WORDS_BIGENDIAN)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/config.h
  )

include_directories("${PROJECT_BINARY_DIR}")

add_subdirectory(src)
add_subdirectory(tests EXCLUDE_FROM_ALL)
