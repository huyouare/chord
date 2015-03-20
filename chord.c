/*
 * chord.c - COMPSCI 512
 *
 */

#include <stdio.h>
#include "csapp.h"
#include <pthread.h>
#include <openssl/sha.h>


#define   FILTER_FILE   "chord.filter"
#define   LOG_FILE      "chord.log"
#define   DEBUG_FILE    "chord.debug"
#define   MAX_RING_SIZE 100

 typedef struct Node {
  char *ip_address;
  char *port;
  char *id;
  int key;
  struct Node *predecessor;
  struct Node *successor;
  int *table;
  // struct Table *table;
} Node;

typedef struct ChordRing {
  int size;
  Node *first;
  Node *last;
  Node *node_array[MAX_RING_SIZE];
} ChordRing;

/*============================================================
 * function declarations
 *============================================================*/

void initialize_chord(int port);
void add_new_node(char *ip_address, char *port);
void receive_client(void *args);

int main(int argc, char *argv[])
{ 
  int chord_port;
  unsigned char hash[SHA_DIGEST_LENGTH];

  if (argc == 2) {
    chord_port = atoi(argv[1]);
    initialize_chord(chord_port);
  } else if (argc == 4) {
    chord_port = atoi(argv[1]);
    printf("Joining the Chord ring...\n");
    add_new_node(argv[2], argv[3]);
    printf("You are listening on port %d\n", chord_port);
    printf("Your position is...\n");
  } else {
    printf("Usage: %s port [nodeAddress nodePort]\n", argv[0]);
    exit(1);
  }
}

void initialize_chord(int port) {
  printf("Creating new Chord ring...\n");
  int listenfd, connfd, optval, clientlen;
  struct sockaddr_in clientaddr;

  listenfd = Open_listenfd(port);
  optval = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
  printf("Listening on port %d\n", port);

  while(1) {

    printf("New client\n");
    clientlen = sizeof(clientaddr); //struct sockaddr_in

    /* accept a new connection from a client here */
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    printf("Connected to client\n");
    // printf("Addr: %d", clientaddr)
    // serverPort = 80; // Default port

    pthread_t thread;

    int *args = malloc(2 * sizeof(int));
    args[0] = connfd;
    args[1] = port;

    if (pthread_create(&thread, NULL, &receive_client, args) < 0) {
      printf("receive_client thread error\n");
    }
    // pthread_join(&thread);

    printf("Done Thread\n");
  }
}

void receive_client(void *args) {

}

void add_new_node(char *ip_address, char *port) {
  char data[strlen(ip_address) + strlen(port) + sizeof(char)];
  data[0] = 0;
  unsigned char hash[SHA_DIGEST_LENGTH];
  strcat(data, ip_address);
  strcat(data, ":");
  strcat(data, port);
  SHA1(data, sizeof(data), hash);
  printf("%s\n", data);
  printf("%s\n", hash);

  uint32_t key;
  memcpy(&key, &hash, sizeof(key));
  printf("%d\n", key);
}

int find_successor(int id, int *table) {

}

