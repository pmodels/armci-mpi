CC     = mpicc
CFLAGS = -I../../include
LDFLAGS= -L../../lib
LIBS   = -larmci -lm

OBJ    = test.o

TARGET = test.x

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $+ $(LIBS)

clean:
	rm -f $(TARGET) *.o
