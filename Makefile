CC      = mpicc
CFLAGS  = -g -O0 -I.
#CFLAGS += -Wall

### For timing runs when you don't care about safety:
# CFLAGS += -DNO_SEATBELTS

OBJ    = armci_util.o           \
         armci_malloc.o         \
         mem_region.o           \
         armci_onesided.o       \
         armci_mutex.o          \
         armci_onesided_nb.o    \
         armci_msg.o            \
         strided.o              \
         vector.o               \
         debug.o                \
         # end


libarmci.a: $(OBJ)
	ar r $@ $+

.PHONY: clean
clean:
	rm -f *.o test libarmci.a
