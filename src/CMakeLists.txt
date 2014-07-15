set(libaec_SRCS encode.c encode_accessors.c decode.c)
add_library(aec SHARED ${libaec_SRCS})
set_target_properties(aec PROPERTIES
  VERSION 0
  SOVERSION 0.0
  )
add_library(sz SHARED sz_compat.c)
set_target_properties(sz PROPERTIES
  VERSION 0
  SOVERSION 0.0
  )
target_link_libraries(sz aec)

add_executable(aec_client aec.c)
set_target_properties(aec_client PROPERTIES OUTPUT_NAME "aec")
target_link_libraries(aec_client aec)
add_executable(utime EXCLUDE_FROM_ALL utime.c)

install(FILES libaec.h szlib.h DESTINATION include)
install(TARGETS aec sz
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  )
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/aec DESTINATION bin)
