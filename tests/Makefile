CC       = mpicc
CFLAGS   = -g -Wall -O0 -I../include

LIBARMCI = ../lib/libarmci.a

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

SUBMAKES = armci_test                   \
           cg                           \
           lu                           \
           transp1D                     \
           non-blocking                 \
           # end

all: $(TESTS) $(SUBMAKES)

$(TESTS): $(% : %.c) $(LIBARMCI)

.PHONY: $(SUBMAKES)
$(SUBMAKES):
	$(MAKE) -C $@

.PHONY: clean
clean:
	rm -f *.o $(TESTS)
	rm -rf $(TESTS:%=%.dSYM)
	@for d in $(SUBMAKES); do make -C $$d clean; done
