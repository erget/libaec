CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
INCLUDE(CheckIncludeFiles)
INCLUDE(TestBigEndian)
INCLUDE(CheckCSourceCompiles)
PROJECT(libaec)
SET(libaec_VERSION_MAJOR 0)
SET(libaec_VERSION_MINOR 2)
SET(CMAKE_BUILD_TYPE Release)
ENABLE_TESTING()

CHECK_INCLUDE_FILES(malloc.h HAVE_MALLOC_H)
CHECK_INCLUDE_FILES(stdint.h HAVE_STDINT_H)
TEST_BIG_ENDIAN(WORDS_BIGENDIAN)
CHECK_C_SOURCE_COMPILES(
  "int main(int argc, char *argv[])
{return __builtin_clzll(1LL)\;}"
  HAVE_DECL___BUILTIN_CLZLL
  )

#Inspired from http://www.cmake.org/Wiki/CMakeTestInline
SET(INLINE_TEST_SRC "/* Inspired by autoconf's c.m4 */
static inline int static_foo(){return 0\;}
int main(int argc, char *argv[]){return 0\;}
")
FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/CMakeTestCInline.c
  ${INLINE_TEST_SRC})

FOREACH(KEYWORD "inline" "__inline__" "__inline")
   IF(NOT DEFINED C_INLINE)
     TRY_COMPILE(C_HAS_${KEYWORD}
       ${CMAKE_CURRENT_BINARY_DIR}
       ${CMAKE_CURRENT_BINARY_DIR}/CMakeTestCInline.c
       COMPILE_DEFINITIONS "-Dinline=${KEYWORD}"
       )
     IF(C_HAS_${KEYWORD})
       SET(C_INLINE TRUE)
       ADD_DEFINITIONS("-Dinline=${KEYWORD}")
     ENDIF(C_HAS_${KEYWORD})
   ENDIF(NOT DEFINED C_INLINE)
ENDFOREACH(KEYWORD)

IF(NOT DEFINED C_INLINE)
   ADD_DEFINITIONS("-Dinline=")
ENDIF(NOT DEFINED C_INLINE)

SET(RESTRICT_TEST_SRC "/* Inspired by autoconf's c.m4 */
int foo (int * restrict ip){return ip[0]\;}
int main(int argc, char *argv[]){int s[1]\;
int * restrict t = s\; t[0] = 0\; return foo(t)\;}
")

FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/CMakeTestCRestrict.c
  ${RESTRICT_TEST_SRC})

FOREACH(KEYWORD "restrict" "__restrict" "__restrict__" "_Restrict")
   IF(NOT DEFINED C_RESTRICT)
     TRY_COMPILE(C_HAS_${KEYWORD}
       ${CMAKE_CURRENT_BINARY_DIR}
       ${CMAKE_CURRENT_BINARY_DIR}/CMakeTestCRestrict.c
       COMPILE_DEFINITIONS "-Drestrict=${KEYWORD}"
       )
     IF(C_HAS_${KEYWORD})
       SET(C_RESTRICT TRUE)
       ADD_DEFINITIONS("-Drestrict=${KEYWORD}")
     ENDIF(C_HAS_${KEYWORD})
   ENDIF(NOT DEFINED C_RESTRICT)
ENDFOREACH(KEYWORD)

IF(NOT DEFINED C_RESTRICT)
   ADD_DEFINITIONS("-Drestrict=")
ENDIF(NOT DEFINED C_RESTRICT)

CONFIGURE_FILE(
  ${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/config.h
  )

INCLUDE_DIRECTORIES("${PROJECT_BINARY_DIR}")
ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(tests EXCLUDE_FROM_ALL)
