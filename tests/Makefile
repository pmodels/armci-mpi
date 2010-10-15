CC       = mpicc
CFLAGS   = -g -Wall -O0 -I../

LIBARMCI = ../libarmci.a
#LIBARMCI = $(HOME)/opt/lib/libarmci.a -lm

TESTS    = test_onesided                \
           test_mutex                   \
           test_mutex_rmw               \
           test_mutex_trylock           \
           test_malloc                  \
           test_malloc_irreg            \
           ARMCI_PutS_latency           \
           ARMCI_PutAccS_latency        \
           test_groups                  \
           test_group_split             \
           test_malloc_group            \
           # end

all: $(TESTS)

$(TESTS): $(% : %.c) $(LIBARMCI)

.PHONY: clean
clean:
	rm -f *.o $(TESTS)
