CC     = mpicc
CFLAGS = -I../../
LDFLAGS= -L../../
LIBS   = -larmci -lm

OBJ    = test.o

TARGET = test.x

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $+ $(LIBS)

clean:
	rm -f $(TARGET) *.o
