CC = gcc
CFLAGS = -g -Wall -pthread

MASTER = master
PALIN = palin

all: master palin

master: master.o
	$(CC) $(CFLAGS) -o $(MASTER) master.o

palin: palin.o
	$(CC) $(CFLAGS) -o $(PALIN) palin.o

master.o: master.c
	$(CC) $(CFLAGS) -c master.c

palin.o: palin.c
	$(CC) $(CFLAGS) -c palin.c

clean:
	rm *.o $(MASTER) $(PALIN) palin.out nopalin.out
