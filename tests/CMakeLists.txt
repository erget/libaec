add_library(check_aec check_aec.c)

add_executable(check_code_options check_code_options.c)
target_link_libraries(check_code_options check_aec aec)

add_executable(check_buffer_sizes check_buffer_sizes.c)
target_link_libraries(check_buffer_sizes check_aec aec)

add_executable(check_long_fs check_long_fs.c)
target_link_libraries(check_long_fs check_aec aec)

add_test(check_code_options check_code_options)
add_test(check_buffer_sizes check_buffer_sizes)
add_test(check_long_fs check_long_fs)

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})
add_dependencies(check
  check_code_options
  check_buffer_sizes
  check_long_fs)

add_custom_target(bench
    COMMAND ${CMAKE_SOURCE_DIR}/tests/benc.sh
    COMMAND ${CMAKE_SOURCE_DIR}/tests/bdec.sh
    DEPENDS aec_client utime
)