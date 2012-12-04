AUTOMAKE_OPTIONS = color-tests
AM_CPPFLAGS = -I$(top_srcdir)/src
TESTS = check_code_options check_buffer_sizes
check_LTLIBRARIES = libcheck_aec.la
libcheck_aec_la_SOURCES = check_aec.c check_aec.h
check_PROGRAMS = check_code_options check_buffer_sizes
check_code_options_SOURCES = check_code_options.c check_aec.h \
$(top_builddir)/src/libaec.h

check_buffer_sizes_SOURCES = check_buffer_sizes.c check_aec.h \
$(top_builddir)/src/libaec.h

LDADD = libcheck_aec.la $(top_builddir)/src/libaec.la
