lib_LTLIBRARIES = libsz-0.0.la libaec-0.0.la
libaec_0_0_la_SOURCES = encode.c encode_accessors.c decode.c encode.h encode_accessors.h
libsz_0_0_la_SOURCES = sz_compat.c
libaec_0_0_la_LDFLAGS = -version-info 0:0
libsz_0_0_la_LDFLAGS = -version-info 0:0
include_HEADERS = libaec.h szlib.h

bin_PROGRAMS = aec test_szcomp

aec_LDADD = libaec-0.0.la
test_szcomp_LDADD = $(lib_LTLIBRARIES)

aec_SOURCES = aec.c
test_szcomp_SOURCES = test_szcomp.c
