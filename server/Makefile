CC=gcc
CFLAGS=-I$(IDIR) -Wall -g

LIBS=-lm

DEPS = helpers.h players.h

OBJ = main.o players.o helpers.o

server: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

.PHONY: clean run check

clean:
	rm -f *.o *~ core

check:
	splint -warnposix -warnposixheaders -unrecog +matchanyintegral main.c

run: server
	./server
