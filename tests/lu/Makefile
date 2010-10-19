CC      = mpicc
CFLAGS  = -g -Wall -O0 -I../../include
LDFLAGS = -L../../lib
LIBS    = -larmci -lm

all: lu lu-block lu-b-bc

lu: lu.o timing.o
	$(CC) $(LDFLAGS) -o $@ $+ $(LIBS)

lu-block: lu-block.o timing.o
	$(CC) $(LDFLAGS) -o $@ $+ $(LIBS)

lu-b-bc: lu-b-bc.o timing.o
	$(CC) $(LDFLAGS) -o $@ $+ $(LIBS)

clean:
	rm -f *.o lu lu-block lu-b-bc
