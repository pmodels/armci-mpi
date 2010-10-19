CC      = mpicc
CFLAGS  = -g -O0 -Iinclude -Isrc
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

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $+

lib/libarmci.a: $(OBJ:%=src/%)
	ar rcs $@ $+
	ranlib $@

.PHONY: clean
clean:
	rm -f src/*.o lib/libarmci.a
