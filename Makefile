all:
	gcc -c csapp.c
	gcc -c chord.c
	gcc -lssl -lcrypto -pthread csapp.o chord.o -o chord
	