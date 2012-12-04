lib_LTLIBRARIES = libaec.la libsz.la
libaec_la_SOURCES = encode.c encode_accessors.c decode.c \
encode.h encode_accessors.h decode.h

libsz_la_SOURCES = sz_compat.c
libsz_la_LIBADD = libaec.la
include_HEADERS = libaec.h szlib.h

bin_PROGRAMS = aec
aec_LDADD = libaec.la
aec_SOURCES = aec.c
