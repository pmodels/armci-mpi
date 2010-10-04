CC     = mpicc
CFLAGS = -I../../../
LDFLAGS= -L../../../
LIBS   = -larmci -lm

TARGET = overlap

$(TARGET): $(TARGET).o
	$(CC) $(LDFLAGS) -o $@ $+ $(LIBS)

clean:
	rm -f $(TARGET) *.o
