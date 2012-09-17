lib_LTLIBRARIES = libae-0.0.la
libae_0_0_la_SOURCES = aee.c aee_accessors.c aed.c aee.h aee_accessors.h sz_compat.c
libae_0_0_la_LDFLAGS = -version-info 0:0
include_HEADERS = libae.h

bin_PROGRAMS = encode decode test_szcomp

encode_LDADD = $(lib_LTLIBRARIES)
decode_LDADD = $(lib_LTLIBRARIES)
test_szcomp_LDADD = $(lib_LTLIBRARIES)

encode_SOURCES = encode.c
decode_SOURCES = decode.c
test_szcomp_SOURCES = test_szcomp.c
