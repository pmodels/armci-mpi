CC     = mpicc
CFLAGS = -I../../
LDFLAGS= -L../../
LIBS   = -larmci -lm

OBJ    = cg.o           \
         compute.o      \
         read_input.o   \
         timing.o       \

TARGET = cg

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $+ $(LIBS)

clean:
	rm -f $(TARGET) *.o
