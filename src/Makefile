CC = gcc
#CFLAGS = -g -pg -lc -O2 -Wall -fprofile-arcs -ftest-coverage -DPROFILE
#CFLAGS = -g -pg -lc -O2 -Wall -fprofile-arcs -ftest-coverage -DUNROLL_BLOCK_8
CFLAGS = -g -O3 -Wall

OBJS = aee.o aed.o sz_compat.o

.PHONY : all clean check

all: libae.a

encode: encode.o libae.a
	$(CC) $(CFLAGS) -o $@ $< -L. -lae

decode: decode.o libae.a
	$(CC) $(CFLAGS) -o $@ $< -L. -lae

test_szcomp: test_szcomp.o libae.a
	$(CC) $(CFLAGS) -o $@ $< -L. -lae

libae.a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)
	-@ ($(RANLIB) $@ || true) >/dev/null 2>&1

aed.o: libae.h
aee.o: libae.h

install: libae.a
	mkdir -p ../lib ../include
	ln -sfT ../src/szlib.h ../include/szlib.h
	ln -sfT ../src/libae.h ../include/libae.h
	ln -sfT ../src/libae.a ../lib/libsz.a

clean:
	rm -f $(OBJS) encode.o decode.o \
	encode decode libae.a \
	test_szcomp test_szcomp.o \
	../data/test.ae ../data/test \
	*.gcno *.gcda *.gcov gmon.out

check: encode decode test_szcomp
	 ./encode -c -b1 -B8 -R128 ../data/example_data > ../data/test.aee
	 ./decode -b1 -B8 -R128 ../data/test.aee
	 diff ../data/test ../data/example_data
	 ./encode -c -b1024 -B8 -R128 ../data/example_data > ../data/test.aee
	 ./decode -b1024 -B8 -R128 ../data/test.aee
	 diff ../data/test ../data/example_data
	 ./test_szcomp 65536 ../data/example_data_16


