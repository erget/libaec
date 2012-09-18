lib_LTLIBRARIES = libaec-0.0.la
libaec_0_0_la_SOURCES = encode.c encode_accessors.c decode.c encode.h encode_accessors.h sz_compat.c
libaec_0_0_la_LDFLAGS = -version-info 0:0
include_HEADERS = libaec.h

bin_PROGRAMS = aec test_szcomp

aec_LDADD = $(lib_LTLIBRARIES)
test_szcomp_LDADD = $(lib_LTLIBRARIES)

aec_SOURCES = aec.c
test_szcomp_SOURCES = test_szcomp.c
