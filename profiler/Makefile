CPPFLAGS = -I../src
LIBS = -L../build/.libs -larmci

CC = mpicc
CFLAGS = -g $(CPPFLAGS)
SOFLAGS =  -shared -fPIC

LD = ld
LDFLAGS = -shared

all: libarmciprof.so profiling.o

libarmciprof.so: profiling.c
	$(CC) $(CFLAGS) $(SOFLAGS) $< -o $@

#libarmciprof.so: profiling.o
#	$(LD) $(LDFLAGS) $< -o $@

profiling.o: profiling.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	-rm -f *.o *.so

