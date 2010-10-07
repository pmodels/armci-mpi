CC     = mpicc
CFLAGS = -I../../
LIBS   = -larmci -lm

#LDFLAGS= -L/home/dinan/opt/lib
LDFLAGS= -L../../

OBJ    = cg.o           \
         compute.o      \
         read_input.o   \
         timing.o       \

TARGET = cg

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $+ $(LIBS)

clean:
	rm -f $(TARGET) *.o
