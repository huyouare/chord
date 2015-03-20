/*
 * chord.c - COMPSCI 512
 *
 */

#include <stdio.h>
#include "csapp.h"
#include <pthread.h>
 #include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/sha.h>

#define   FILTER_FILE   "chord.filter"
#define   LOG_FILE      "chord.log"
#define   DEBUG_FILE    "chord.debug"
#define   MAX_RING_SIZE 100
#define   KEY_SIZE      32
#define   KEY_SPACE     4294967296
#define   MAX_MSG_LENGTH 512
#define   MAX_BACK_LOG  512
#define   LOCAL_IP_ADDRESS "127.0.0.1" 

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
void add_new_node(char *ip_address, char *port_str);
void receive_client(void *args);
int server(uint16_t port);
int client(const char * addr, uint16_t port);
uint32_t find_successor(int id);

int ring_size;
int finger_table[KEY_SIZE];

int main(int argc, char *argv[])
{ 
  int chord_port, node_port;
  unsigned char hash[SHA_DIGEST_LENGTH];

  if (argc == 2) {
    chord_port = atoi(argv[1]);
    initialize_chord(chord_port);
  } else if (argc == 4) {
    chord_port = atoi(argv[1]);
    node_port = atoi(argv[3]);
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

  while(1) {
    printf("New client\n");
    clientlen = sizeof(clientaddr); //struct sockaddr_in

    /* accept a new connection from a client here */
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    printf("Connected to client\n");

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
  int byteCount = 0;
  char buf1[MAXLINE], buf2[MAXLINE], buf3[MAXLINE];
  rio_t server, client;
  
  clientfd = ((int*)args)[0];
  serverPort = ((int*)args)[1];
  free(args);

  char request[MAXLINE];

  /* Read first line of request */
  Rio_readinitb(&client, clientfd);
  numBytes = Rio_readlineb(&client, request, MAXLINE);

  if (numBytes <= 0) {
    printf("No request received\n");
  }
  printf("Request: %s\n", request);

  // while (Rio_readlineb(&client, buf1, MAXLINE) > 0) {
  //   printf("%s\n", buf1);
  // }

  /* Check if query or node connection */

  /* new node */
  if (strncmp(request, "new", 3) == 0) {
    uint32_t key = 0;
    key = (uint32_t) atoi(request+3);
    printf("Key: %u\n", key);

    /* Find out immediate successors/predecessors */

    uint32_t fingers[KEY_SIZE];
    /* Lookup fingers of new node */
    int i = 0;
    int product = 1;
    /* n+2^i where i = 0..<m */
    for (i = 0; i < KEY_SIZE; i++) {
      fingers[i] = find_successor( (key + product) % KEY_SPACE );
      if (i > 0 && fingers[i] != fingers[i-1]) {
        update_node(fingers[i]);
      }
      product = product * 2;
      printf("finger %d: %u\n", i, fingers[i]);
    }
  }

  /* finger table request */
  if (strncmp(request, "table", 5) == 0) {
    
  }
}

void node_process() {

}

void node_listen() {

}

void add_new_node(char *ip_address, char *port_str) {
  uint32_t key = 0;
  int port = atoi(port_str);
  int serverfd;

  char data[strlen(ip_address) + strlen(port_str) + sizeof(char)];
  data[0] = 0;
  unsigned char hash[SHA_DIGEST_LENGTH];
  strcat(data, ip_address);
  strcat(data, ":");
  strcat(data, port_str);
  SHA1(data, sizeof(data), hash);
  printf("%s\n", data);
  printf("%s\n", hash);
  memcpy(&key, hash + 16, sizeof(key));
  printf("%u\n", key);

  printf("Joining the Chord ring...\n");

  int sock;
  struct sockaddr_in server_addr;

  if ((sock = socket(AF_INET, SOCK_STREAM/* use tcp */, 0)) < 0) {
    perror("Create socket error:");
  }

  printf("Socket created\n");
  server_addr.sin_addr.s_addr = inet_addr(ip_address);
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);

  if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
    perror("Connect error:");
  }
  printf("Connected to server %s:%d\n", ip_address, port);

  char request_string[MAXLINE];
  request_string[0] = 0;
  strcat(request_string, "new");
  sprintf(request_string+3, "%u\0", key);

  printf("%s\n", request_string);
  if (send(sock, request_string, MAX_MSG_LENGTH, 0) < 0) {
    perror("Send error:");
  }

  // while (1) {
  //   printf("%s", request_string);
  //   if (send(sock, request_string, MAXLINE, 0) < 0) {
  //     perror("Send error");
  //   }
  // }

  // Add successor/predecessor
  // Node *head = null;

  // printf("You are listening on port %d\n", chord_port);
  printf("Your position is...\n");

}

uint32_t find_successor(int key) {
  uint32_t table[] = finger_table;

  while (table [0] < key) {
    int i = 0;
    while (table[i] < key) {
      i++;
    }
    i--;
    uint32_t node = finger[i];
    fetch_table(finger, &table);
  }

  return table[0];
}

uint32_t find_prececessor(int key, int *table) {

}

void update_node(int key) {

}

