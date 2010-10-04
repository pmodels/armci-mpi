CC     = mpicc
CFLAGS = -g -Wall -O0 -I.

### For timing runs when you don't care about safety:
# CFLAGS += -DNO_SEATBELTS

OBJ    = armci_util.o     \
         armci_malloc.o   \
         seg_hdl.o        \
         armci_onesided.o \
         armci_mutex.o    \
         armci_onesided_nb.o \
         armci_msg.o      \
         strided.o        \
         debug.o


libarmci.a: $(OBJ)
	ar r $@ $+

.PHONY: clean
clean:
	rm -f *.o test libarmci.a
