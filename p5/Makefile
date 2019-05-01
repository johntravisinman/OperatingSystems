CC = gcc
CFLAGS = -g -Wall -lm

OSS = oss
USER = user

all: oss user

oss: main.o
	$(CC) $(CFLAGS) -o $(OSS) main.o

main.o: main.c oss.h msg.h simtime.h resourcedescriptor.h
	$(CC) $(CFLAGS) -c main.c

user: user.o
	$(CC) $(CFLAGS) -o $(USER) user.o

user.o: user.c oss.h msg.h simtime.h resourcedescriptor.h
	$(CC) $(CFLAGS) -c user.c

clean:
	rm *.o oss user
