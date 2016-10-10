ADD_LIBRARY(check_aec STATIC check_aec.c)

ADD_EXECUTABLE(check_code_options check_code_options.c)
TARGET_LINK_LIBRARIES(check_code_options check_aec aec)
ADD_TEST(NAME check_code_options COMMAND check_code_options)
ADD_EXECUTABLE(check_buffer_sizes check_buffer_sizes.c)
TARGET_LINK_LIBRARIES(check_buffer_sizes check_aec aec)
ADD_TEST(NAME check_buffer_sizes COMMAND check_buffer_sizes)
ADD_EXECUTABLE(check_long_fs check_long_fs.c)
TARGET_LINK_LIBRARIES(check_long_fs check_aec aec)
ADD_TEST(NAME check_long_fs COMMAND check_long_fs)
ADD_EXECUTABLE(check_szcomp check_szcomp.c)
TARGET_LINK_LIBRARIES(check_szcomp check_aec sz)
ADD_TEST(NAME check_szcomp
  COMMAND check_szcomp ${CMAKE_CURRENT_SOURCE_DIR}/../data/121B2TestData/ExtendedParameters/sar32bit.dat)

IF(UNIX)
  ADD_TEST(
    NAME sampledata.sh
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/sampledata.sh ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
ENDIF(UNIX)
