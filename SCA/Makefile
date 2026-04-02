CC = gcc
CFLAGS = -Wall -g -pthread
LDFLAGS = -lrt

all: SCAser SCAges SCActl

SCAser: SCAser.o
	$(CC) $(CFLAGS) -o SCAser SCAser.o $(LDFLAGS)

SCAser.o: SCAser.c common.h monitor.h
	$(CC) $(CFLAGS) -c SCAser.c

SCAges: SCAges.o
	$(CC) $(CFLAGS) -o SCAges SCAges.o $(LDFLAGS)

SCActl: SCActl.o
	$(CC) $(CFLAGS) -o SCActl SCActl.o $(LDFLAGS)

%.o: %.c common.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o SCAser SCAges SCActl USERS.DAT ACESSOS.LOG /tmp/SCAges_* /tmp/SERV
