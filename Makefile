all:
	gcc -c csapp.c
	gcc -c chord.c
	gcc -c query.c
	gcc -lssl -lcrypto -pthread csapp.o chord.o -o chord
	gcc -lssl -lcrypto -pthread csapp.o query.o -o query
	