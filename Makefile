CC = gcc
CFLAGS = -Wall -g
TARGET = ConstanteD-clienteFTP
OBJS = ConstanteD-clienteFTP.o connectTCP.o connectsock.o errexit.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

ConstanteD-clienteFTP.o: ConstanteD-clienteFTP.c
	$(CC) $(CFLAGS) -c ConstanteD-clienteFTP.c

connectTCP.o: connectTCP.c
	$(CC) $(CFLAGS) -c connectTCP.c

connectsock.o: connectsock.c
	$(CC) $(CFLAGS) -c connectsock.c

errexit.o: errexit.c
	$(CC) $(CFLAGS) -c errexit.c

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
