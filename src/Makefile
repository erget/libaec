CC = gcc
CFLAGS = -g -Wall

objects = mytest.o aed.o

mytest: $(objects)
	$(CC) $(CFLAGS) -o mytest $(objects)

aed.o: aecd.h

.PHONY : clean test vtest
clean:
	rm -f mytest ../data/ae_out $(objects)

test: mytest
	./mytest 1 1 < ../data/example_data.szip > ../data/ae_out
	diff ../data/ae_out ../data/example_data
	./mytest 9478 16384 < ../data/example_data.szip > ../data/ae_out
	diff ../data/ae_out ../data/example_data

vtest: mytest
	valgrind -v ./mytest < ../data/example_data.szip > ../data/ae_out
	diff ../data/ae_out ../data/example_data
