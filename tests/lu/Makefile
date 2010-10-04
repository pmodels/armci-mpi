CC=mpicc
CFLAGS=-g -Wall -O0 -I../../
LDFLAGS=-L../../
LIBS=-larmci -lm

lu: lu.o timing.o
	$(CC) $(LDFLAGS) -o $@ $+ $(LIBS)

clean:
	rm -f *.o lu
