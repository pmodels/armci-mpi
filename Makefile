CC      = mpicc
CFLAGS  = -g -O0 -I.
CFLAGS += -Wall

### For timing runs when you don't care about safety:
# CFLAGS += -DNO_SEATBELTS

OBJ    =  debug.o               \
          groups.o              \
          malloc.o              \
          mem_region.o          \
          message.o             \
          message_gop.o         \
          mutex.o               \
          mutex_grp_notify.o    \
          onesided.o            \
          onesided_nb.o         \
          rmw.o                 \
          strided.o             \
          topology.o            \
          util.o                \
          value_ops.o           \
          vector.o              \
         #mutex_grp_spin.o      \
         # end

libarmci.a: $(OBJ)
	ar rcs $@ $+
	ranlib $@

.PHONY: clean
clean:
	rm -f *.o test libarmci.a
