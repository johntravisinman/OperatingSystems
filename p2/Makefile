CC = gcc
CFLAGS = -g -Wall

TARGET1 = oss
TARGET2 = worker

all: oss worker

oss: master.o
	$(CC) $(CFLAGS) -o $(TARGET1) master.o

worker: worker.o
	$(CC) $(CFLAGS) -o $(TARGET2) worker.o

master.o: master.c times.h
	$(CC) $(CFLAGS) -c master.c

worker.o: worker.c times.h
	$(CC) $(CFLAGS) -c worker.c

clean:
	rm *.o 
	rm $(TARGET1) 
	rm $(TARGET2)