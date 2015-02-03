CC=gcc

FLAGS=-Wall -g

DEPS = helpers.h players.h

OBJ = main.o players.o helpers.o

server: $(OBJ)
	$(CC) $(FLAGS) -o $@ $^

%.o: %.c $(DEPS)
	$(CC) $(FLAGS) -c -o $@ $<

.PHONY: clean run check

clean:
	rm -f *.o *~ core

check:
	splint -warnposix -warnposixheaders -unrecog +matchanyintegral main.c

run: server
	./server
