CC = gcc
CFLAGS = -g -Wall

OBJS = aee.o aed.o

.PHONY : all clean test

all: libae.a test

test_encode: test_encode.o libae.a
	$(CC) $(CFLAGS) -o test_encode test_encode.o -L. -lae

test_decode: test_decode.o libae.a
	$(CC) $(CFLAGS) -o test_decode test_decode.o -L. -lae

libae.a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)
	-@ ($(RANLIB) $@ || true) >/dev/null 2>&1

aed.o: libae.h
aee.o: libae.h

clean:
	rm -f $(OBJS) test_encode.o test_decode.o \
	test_encode test_decode libae.a \
	../data/test.ae ../data/test

test: test_encode test_decode
	./test_encode 1 1 < ../data/example_data > ../data/test.ae
	./test_decode 1 1 < ../data/test.ae > ../data/test
	diff ../data/test ../data/example_data
