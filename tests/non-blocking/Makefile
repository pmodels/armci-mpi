CC     = mpicc
CFLAGS = -I../../
LDFLAGS= -L../../
LIBS   = -larmci -lm

TARGETS = simple overlap

all: $(TARGETS)

simple: simple.o
	$(CC) $(LDFLAGS) -o $@ $+ $(LIBS)

overlap: overlap.o
	$(CC) $(LDFLAGS) -o $@ $+ $(LIBS)

clean:
	rm -f $(TARGETS) *.o
