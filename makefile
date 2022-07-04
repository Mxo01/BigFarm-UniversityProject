CC=gcc
CFLAGS=-g -Wall -O -std=c99
LDLIBS=-lm -lrt -pthread

all: farm client

farm: farm.o xerrori.o
client: client.o xerrori.o

# target che cancella eseguibili e file oggetto
clean:
	rm -f all *.o