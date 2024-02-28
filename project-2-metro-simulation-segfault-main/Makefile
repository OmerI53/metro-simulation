CC = gcc
CFLAGS = -Wall -pthread

all: sim

sim: simulation.c
	$(CC) $(CFLAGS) -o sim simulation.c

clean:
	rm -f sim
