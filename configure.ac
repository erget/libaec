AC_PREREQ([2.68])
AC_INIT([libaec], [0.0.2], [rosenhauer@dkrz.de])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_SRCDIR([src/libaec.h])
AC_CONFIG_HEADERS(config/config.h)

LT_INIT

AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX

# Checks for libraries.
AC_CHECK_LIB([aec], [aec_decode])

# Checks for header files.
AC_CHECK_HEADERS([inttypes.h stddef.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_CHECK_FUNCS([memset strstr])

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 tests/Makefile])
AC_C_BIGENDIAN

AC_OUTPUT
