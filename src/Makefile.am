noinst_LIBRARIES = libae.a
libae_a_SOURCES = aee.c aee_mutators.c aed.c sz_compat.c
include_HEADERS = libae.h

bin_PROGRAMS = encode decode test_szcomp

encode_LDADD = libae.a
decode_LDADD = libae.a
test_szcomp_LDADD = libae.a

encode_SOURCES = encode.c
decode_SOURCES = decode.c
test_szcomp_SOURCES = test_szcomp.c
