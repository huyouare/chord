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
#define   KEY_SIZE      32
#define   KEY_SPACE     32

typedef struct Finger
{
  struct Node *node;
  int key;
} Finger;

typedef struct Table 
{
  struct Finger **finger_array;
  int length;
} Table;

typedef struct Node 
{
  char *ip_address;
  char *port;
  char *id;
  int key;
  struct Node *predecessor;
  struct Node *pre_second;
  struct Node *successor;
  struct Node *suc_second;
  int *table;
  // struct Table *table;
} Node;

typedef struct ChordRing 
{
  int size;
  char *ip_address;
  int port;
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

int ring_size;

int main(int argc, char *argv[])
{ 
  int chord_port;
  unsigned char hash[SHA_DIGEST_LENGTH];

  if (argc == 2) {
    chord_port = atoi(argv[1]);
    initialize_chord(chord_port);
  } else if (argc == 4) {
    chord_port = atoi(argv[1]);
    add_new_node(argv[2], argv[3]);
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

  Node *first;
  Node *last;
  Node *node_array[MAX_RING_SIZE];

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
  int numBytes, lineNum, serverfd, clientfd, serverPort;
  int numBytes1, numBytes2;
  int tries;
  int byteCount = 0;
  char buf1[MAXLINE], buf2[MAXLINE], buf3[MAXLINE];
  char host[MAXLINE];
  char url[MAXLINE], logString[MAXLINE];
  char *token, *cmd, *version, *file, *saveptr;
  rio_t server, client;
  char slash[10];
  strcpy(slash, "/");
  
  clientfd = ((int*)args)[0];
  serverPort = ((int*)args)[1];
  free(args);

  char request[MAXLINE];
  char buf[MAXLINE];

  /* Read first line of request */
  Rio_readinitb(&client, clientfd);
  numBytes = Rio_readlineb(&client, request, MAXLINE);

  if (numBytes <= 0) {
    printf("No request received\n");
  }

  printf("Request: %s", request);

  char *uri;
  char target_address[MAXLINE];
  char path[MAXLINE];
  int *port = &serverPort;
  char request_copy[MAXLINE];

  int *keep_alive = malloc(sizeof(int *));
  *keep_alive = 0;

  /* Create separate variables for URI */
  strcpy(request_copy, request);
  uri = strchr(request, ' ');
  ++uri;
  strtok_r(uri, " ", &saveptr);
  printf("URI %s\n", uri);


  /* Check if query or node connection */
}

void node_process() {

}

void node_listen() {

}

void add_new_node(char *ip_address, char *port_str) {
  uint32_t key;
  int port = atoi(port_str);
  int serverfd;

  char data[strlen(ip_address) + strlen(port_str) + sizeof(char)];
  data[0] = 0;
  unsigned char hash[SHA_DIGEST_LENGTH];
  strcat(data, ip_address);
  strcat(data, ":");
  strcat(data, port);
  SHA1(data, sizeof(data), hash);
  printf("%s\n", data);
  printf("%s\n", hash);

  memcpy(&key, &hash + 16, sizeof(key));
  printf("%d\n", key);

  printf("Joining the Chord ring...\n");

  serverfd = Open_clientfd(ip_address, port);
  char request_string[MAXLINE];
  request_string[0] = 0;
  strcat(request_string, "new");
  send(serverfd, request_string, MAXLINE, 0);

  // Add successor/predecessor
  // Node *head = null;

  // printf("You are listening on port %d\n", chord_port);
  printf("Your position is...\n");

}

int find_successor(int id, int *table) {

}

int find_prececessor(int id, int *table) {

}

