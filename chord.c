/*
 * chord.c - COMPSCI 512
 *
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
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
#define   MAX_BACK_LOG  512
#define   LOCAL_IP_ADDRESS "127.0.0.1" 

typedef struct Table 
{
  struct Finger **finger_array;
  int length;
} Table;

typedef struct Node 
{
  uint32_t key;
  char ip_address[12];
  int port;
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
void add_new_node(char *ip_address, int port, int node_port);
void begin_listening(int port);
void receive_client(void *args);
void update_node(Node node);

Node find_successor(uint32_t key);
Node find_prececessor(uint32_t key);
Node closest_preceding_finger(uint32_t key);
bool is_between(uint32_t key, uint32_t a, uint32_t b);

Node fetch_successor(Node n);
Node fetch_predecessor(Node n);
Node query_successor(uint32_t key, Node n);
Node query_predecessor(uint32_t key, Node n);
Node query_closest_preceding_finger(uint32_t key, Node n);

uint32_t hash_address(char *ip_address, int port);
void print_node(Node n);
Node send_request(Node n, char *message);

int ring_size;
Node self_node;
Node self_predecessor;
Node self_successor;
Node self_finger_table[KEY_SIZE];

int main(int argc, char *argv[])
{ 
  int z;  /* Status code */
  int s;     /* Socket s */
  struct linger so_linger;

  int chord_port, port, node_port;
  unsigned char hash[SHA_DIGEST_LENGTH];

  if (argc == 2) {
    chord_port = atoi(argv[1]);
    initialize_chord(chord_port);
  } else if (argc == 4) {
    port = atoi(argv[1]);
    node_port = atoi(argv[3]);
    add_new_node(argv[2], port, node_port);
  } else {
    printf("Usage: %s port [nodeAddress nodePort]\n", argv[0]);
    exit(1);
  }
}

void initialize_chord(int port) {
  printf("Creating new Chord ring...\n");

  strcpy(self_node.ip_address, LOCAL_IP_ADDRESS);
  self_node.port = port;
  self_node.key = hash_address(LOCAL_IP_ADDRESS, port);
  /* Set self to predecessor and successor */
  self_predecessor = self_node;
  self_successor = self_node;

  /* Set self for fingers */
  int i;
  for (i = 0; i < KEY_SIZE; i++) {
    self_finger_table[i] = self_node;
  }

  begin_listening(port);
}

void begin_listening(int port) {
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
  request[0] = 0;

  printf("Waiting for client request...\n");
  /* Read first line of request */
  numBytes = Rio_readn(clientfd, request, MAXLINE);
  if (numBytes <= 0) {
    printf("No request received\n");
  }
  printf("Request: %s\n", request);

  /* Check if query or node connection */

  /* new node */
  if (strncmp(request, "new", 3) == 0) {
    uint32_t key = 0;
    key = (uint32_t) atoi(request+3);
    printf("Key: %u\n", key);

    /* Find out immediate successors/predecessors */

    Node fingers[KEY_SIZE];
    /* Lookup fingers of new node */
    int i = 0;
    int product = 1;
    /* n+2^i where i = 0..<m */
    for (i = 0; i < KEY_SIZE; i++) {
      fingers[i] = find_successor( (key + product) % KEY_SPACE );
      if (i > 0 && fingers[i].key != fingers[i-1].key) {
        update_node(fingers[i]);
      }
      product = product * 2;
      printf("finger %d: %u\n", i, fingers[i]);
    }
  }

  /* fetch node's successor */
  if (strncmp(request, "fetch_suc", 9) == 0) {
    printf("Handing fetch_suc\n");
    buf1[0] = 0;
    sprintf(buf1, "%u\n", self_successor.key);
    strcat(buf1, self_successor.ip_address);
    strcat(buf1, "\n");
    sprintf(buf2, "%d\n", self_successor.port);
    strcat(buf1, buf2);
    if (rio_writen(clientfd, buf1, MAXLINE) < 0) {
      perror("Send error:");
    }

    shutdown(clientfd, SHUT_WR);
    printf("Response sent.\n");
  }

  /* ask node for successor of key */
  if (strncmp(request, "query_suc", 9) == 0) {
    printf("Handing query_suc\n");
    uint32_t key = 0;
    key = (uint32_t) atoi(request+9);
    printf("%u\n", key);

    Node successor = find_successor(key);
    print_node(successor);

    buf1[0] = 0;
    sprintf(buf1, "%u\n", successor.key);
    strcat(buf1, successor.ip_address);
    strcat(buf1, "\n");
    sprintf(buf2, "%d\n", successor.port);
    strcat(buf1, buf2);
    if (rio_writen(clientfd, buf1, MAXLINE) < 0) {
      perror("Send error:");
    }

    shutdown(clientfd, SHUT_WR);
    printf("Response sent.\n");
  }

  /* ask node for predecessor of key */
  if (strncmp(request, "query_pre", 9) == 0) {
    printf("Handing query_pre\n");
    uint32_t key = 0;
    key = (uint32_t) atoi(request+9);
    printf("%u\n", key);

    Node predecessor = find_prececessor(key);
    print_node(predecessor);

    buf1[0] = 0;
    sprintf(buf1, "%u\n", predecessor.key);
    strcat(buf1, predecessor.ip_address);
    strcat(buf1, "\n");
    sprintf(buf2, "%d\n", predecessor.port);
    strcat(buf1, buf2);
    if (rio_writen(clientfd, buf1, MAXLINE) < 0) {
      perror("Send error:");
    }

    shutdown(clientfd, SHUT_WR);
    printf("Response sent.\n");
  }

  /* ask node for closest preceding finger of key */
  if (strncmp(request, "query_cpf", 9) == 0) {
    printf("Handing query_cpf\n");
    uint32_t key = 0;
    key = (uint32_t) atoi(request+9);
    printf("%u\n", key);

    Node cpf = closest_preceding_finger(key);
    print_node(cpf);

    buf1[0] = 0;
    sprintf(buf1, "%u\n", cpf.key);
    strcat(buf1, cpf.ip_address);
    strcat(buf1, "\n");
    sprintf(buf2, "%d\n", cpf.port);
    strcat(buf1, buf2);
    if (rio_writen(clientfd, buf1, MAXLINE) < 0) {
      perror("Send error:");
    }

    shutdown(clientfd, SHUT_WR);
    printf("Response sent.\n");
  }
}

uint32_t hash_address(char *ip_address, int port) {
  char port_str[5];
  unsigned char hash[SHA_DIGEST_LENGTH];
  hash[0] = 0;
  uint32_t key;
  port_str[0] = 0;
  sprintf(port_str, "%d\0", port);
  printf("%s\n", port_str);
  char data[strlen(ip_address) + strlen(port_str) + sizeof(char)];
  data[0] = 0;
  strcat(data, ip_address);
  strcat(data, ":");
  strcat(data, port_str);
  SHA1(data, sizeof(data), hash);
  printf("%s\n", data);
  printf("%s\n", hash);
  memcpy(&key, hash + 16, sizeof(key));
  printf("%u\n", key);
  return key;
}

void add_new_node(char *ip_address, int port, int node_port) {
  uint32_t key = 0;
  int serverfd;

  key = hash_address(ip_address, port);

  printf("Joining the Chord ring...\n");

  int sock;
  struct sockaddr_in server_addr;

  if ((sock = socket(AF_INET, SOCK_STREAM/* use tcp */, 0)) < 0) {
    perror("Create socket error:");
  }

  server_addr.sin_addr.s_addr = inet_addr(ip_address);
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(node_port);

  if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
    perror("Connect error:");
  }
  printf("Connected to server %s:%d\n", ip_address, node_port);
  
  strcpy(self_node.ip_address, LOCAL_IP_ADDRESS);
  self_node.port = port;
  self_node.key = key;

  /* Initialize predecessor, successor */
  Node fetch_node;
  strcpy(fetch_node.ip_address, ip_address);
  fetch_node.port = node_port;
  fetch_node.key = key;

  self_successor = query_successor(key, fetch_node);
  print_node(self_successor);

  /* Initialize finger table */
  int i = 0;
  int product = 1;
  /* n+2^i where i = 0..<m */
  for (i = 0; i < KEY_SIZE; i++) {
    self_finger_table[i] = query_successor((key + product) % KEY_SPACE, fetch_node);
    // if (i > 0 && fingers[i].key != fingers[i-1].key) {
    //   update_node(fingers[i]);
    // }
    product = product * 2;
    printf("finger %d\n", i);
    print_node(self_finger_table[i]);
  }

  printf("You are listening on port %d\n", port);
  printf("Your position is %u\n", self_node.key);
  printf("Your predecessor is node %s, port %d, position %u\n", self_predecessor.ip_address, self_predecessor.port, self_predecessor.key);
  printf("Your successor is node %s, port %d, position %u\n", self_successor.ip_address, self_successor.port, self_successor.key);
  begin_listening(port);
}

Node find_successor(uint32_t key) {
  Node n = find_prececessor(key);
  return fetch
}

Node find_prececessor(uint32_t key) {
  if (self_node.key == self_successor.key) {
    return self_node;
  }
  Node n = self_node;
  Node suc = self_successor;
  while (!is_between(key, n.key, suc.key)) {
    n = query_closest_preceding_finger(key, n);
    suc = 
  }
}

Node closest_preceding_finger(uint32_t key) {
  int i;
  for (i = KEY_SIZE - 1; i >= 0; i--) {
    printf("Node %d: \n", i);
    print_node(self_finger_table[i]);
    if (is_between(key, self_finger_table[i].key, self_node.key)) {
      return self_finger_table[i];
    }
  }
  return self_node;
}

bool is_between(uint32_t key, uint32_t a, uint32_t b) {
  if (a < key) {
    if (b < a) { // wrap around
      return true;
    } else {
      if (key < b) {
        return true;
      }
    }
  } else {
    if (key < b && b < a) {
      return true;
    }
  }
  return false;
}

Node fetch_successor(Node n) {
  char request_string[MAXLINE];
  request_string[0] = 0;
  strcat(request_string, "fetch_suc");
  return send_request(n, &request_string);
}

Node fetch_predecessor(Node n) {
  char request_string[MAXLINE];
  request_string[0] = 0;
  strcat(request_string, "fetch_pre");
  return send_request(n, &request_string);
}

Node query_predecessor(uint32_t key, Node n) {
  char request_string[MAXLINE];
  request_string[0] = 0;
  strcat(request_string, "query_pre");
  sprintf(request_string+9, "%u\0", key);
  return send_request(n, &request_string);
}

Node query_successor(uint32_t key, Node n) {
  char request_string[MAXLINE];
  request_string[0] = 0;
  strcat(request_string, "query_suc");
  sprintf(request_string+9, "%u\0", key);
  return send_request(n, &request_string);
}

Node query_closest_preceding_finger(uint32_t key, Node n) {
  char request_string[MAXLINE];
  request_string[0] = 0;
  strcat(request_string, "query_cpf");
  sprintf(request_string+9, "%u\0", key);
  return send_request(n, &request_string);
}

void update_node(Node node) {

}

Node send_request(Node n, char *message) {
  Node return_node;
  int sock;
  struct sockaddr_in server_addr;
  rio_t server;

  if ((sock = socket(AF_INET, SOCK_STREAM/* use tcp */, 0)) < 0) {
    perror("Create socket error:");
  }

  server_addr.sin_addr.s_addr = inet_addr(n.ip_address);
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(n.port);

  if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
    perror("Connect error:");
  }
  printf("Connected to server %s:%d\n", n.ip_address, n.port);

  int numBytes;
  char response[MAXLINE];
  printf("Message: %s\n", message);
  if (send(sock, message, MAXLINE,0) < 0) {
    perror("Send error:");
  }
  shutdown(sock, SHUT_WR);

  Rio_readinitb(&server, sock);
  numBytes = Rio_readlineb(&server, response, MAXLINE);
  if (numBytes <= 0) {
    printf("No response received\n");
  } else {
    return_node.key = (uint32_t) atoi(response);
  }

  response[0] = 0;
  numBytes = Rio_readlineb(&server, response, MAXLINE);
  if (numBytes <= 0) {
    printf("No response received\n");
  } else {
    int len = strlen(response);
    if (len > 0 && response[len-1] == '\n') response[len-1] = '\0';
    strcpy(return_node.ip_address, response);
  }

  response[0] = 0;
  numBytes = Rio_readlineb(&server, response, MAXLINE);
  if (numBytes <= 0) {
    printf("No response received\n");
  } else {
    return_node.port = atoi(response);
  }

  return return_node;
}

void print_node(Node n) {
  printf("Key: %u\n", n.key);
  printf("IP: %s\n", n.ip_address);
  printf("port: %d\n", n.port);
}

