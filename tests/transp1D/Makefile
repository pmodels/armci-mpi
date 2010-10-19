CC     = mpicc
CFLAGS = -I../../include
LDFLAGS= -L../../lib
LIBS   = -larmci -lm

TARGET = transp1D-c

$(TARGET): $(TARGET).o
	$(CC) $(LDFLAGS) -o $@ $+ $(LIBS)

clean:
	rm -f $(TARGET) *.o
