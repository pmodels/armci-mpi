CC       = mpicc
CFLAGS   = -g -Wall -O0

#CFLAGS  += -DDIRECT_ACCESS # Enable direct access to shared buffers
CFLAGS  += -I../
LIBARMCI = ../libarmci.a

#CFLAGS  += -I$(HOME)/src/ga-5-0/armci/src
#LIBARMCI = $(HOME)/opt/lib/libarmci.a -lm

TESTS    = ping-pong                    \
	   ping-pong-mpi		\
           ring-flood                   \
           # end

all: $(TESTS)

$(TESTS): $(% : %.c) $(LIBARMCI)

.PHONY: clean
clean:
	rm -f *.o $(TESTS)
