lib_LTLIBRARIES = libsz.la libaec.la
libaec_la_SOURCES = encode.c encode_accessors.c decode.c encode.h encode_accessors.h
libsz_la_SOURCES = sz_compat.c libaec.la
libsz_la_LIBADD = libaec.la
libaec_la_LDFLAGS = -version-info 0:0
libsz_la_LDFLAGS = -version-info 0:0
include_HEADERS = libaec.h szlib.h

bin_PROGRAMS = aec test_szcomp

aec_LDADD = libaec.la
test_szcomp_LDADD = libsz.la

aec_SOURCES = aec.c
test_szcomp_SOURCES = test_szcomp.c
