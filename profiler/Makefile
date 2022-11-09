CC = $${MPICC}
CPPFLAGS = -I../src -I../build/src
CFLAGS = -fPIC $(CPPFLAGS)

LD = ld
LDFLAGS = -shared

all: libarmciprof.so

libarmciprof.so: profiling.o
	$(LD) $(LDFLAGS) $< -o $@

profiling.o: profiling.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	-rm -f *.o *.so

