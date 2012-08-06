CC = gcc
#CFLAGS = -g -Wall -DPROFILE
CFLAGS = -g -Wall

OBJS = aee.o aed.o sz_compat.o

.PHONY : all clean test test_sz

all: libae.a

test_encode: test_encode.o libae.a
	$(CC) $(CFLAGS) -o test_encode test_encode.o -L. -lae

test_decode: test_decode.o libae.a
	$(CC) $(CFLAGS) -o test_decode test_decode.o -L. -lae

test_szcomp: test_szcomp.o libae.a
	$(CC) $(CFLAGS) -o test_szcomp test_szcomp.o -L. -lae

libae.a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)
	-@ ($(RANLIB) $@ || true) >/dev/null 2>&1

aed.o: libae.h
aee.o: libae.h

install: libae.a
	ln -sfT ../src/szlib.h ../include/szlib.h
	ln -sfT ../src/libae.h ../include/libae.h
	ln -sfT ../src/libae.a ../lib/libsz.a

clean:
	rm -f $(OBJS) test_encode.o test_decode.o \
	test_encode test_decode libae.a \
	test_szcomp test_szcomp.o \
	../data/test.ae ../data/test

test: test_encode test_decode
	./test_encode 1 1 < ../data/example_data > ../data/test.ae
	./test_decode 1 1 < ../data/test.ae > ../data/test
	diff ../data/test ../data/example_data
	./test_encode 99 99 < ../data/example_data > ../data/test.ae
	./test_decode 101 101 < ../data/test.ae > ../data/test
	diff ../data/test ../data/example_data

test_sz: test_szcomp
	 ./test_szcomp 65536 < ../data/example_data_16 > ../data/test
	 diff ../data/test ../data/example_data_16

