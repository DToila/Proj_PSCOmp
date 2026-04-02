CC = gcc
CFLAGS = -Wall -g -pthread
LDFLAGS = -lrt

all: SCAser SCAges SCActl

SCAser: SCAser.o
	$(CC) $(CFLAGS) -o SCAser SCAser.o $(LDFLAGS)

SCAser.o: SCAser.c comum.h
	$(CC) $(CFLAGS) -c SCAser.c

SCAges: cmd.o monitor.o comando.o
	$(CC) $(CFLAGS) -o SCAges cmd.o monitor.o comando.o $(LDFLAGS)

SCActl: SCActl.o
	$(CC) $(CFLAGS) -o SCActl SCActl.o $(LDFLAGS)

%.o: %.c comum.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o SCAser SCAges SCActl USERS.DAT ACESSOS.LOG /tmp/SCAges_* /tmp/SERV
