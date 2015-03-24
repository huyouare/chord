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
#define   KEY_SIZE      32
#define   KEY_SPACE     4294967296
#define   LOCAL_IP_ADDRESS "127.0.0.1" 

typedef struct Node 
{
  uint32_t key;
  char ip_address[12];
  int port;
} Node;

/*============================================================
 * function declarations
 *============================================================*/

void initialize_chord(int port);
void join_node(char *ip_address, int node_port, int listen_port);
void begin_listening(int port);
void* receive_client(void *args);

Node find_successor(uint32_t key);
Node find_predecessor(uint32_t key);
Node closest_preceding_finger(uint32_t key);
bool is_between(uint32_t key, uint32_t a, uint32_t b);

void update_successor(Node successor);
void update_predecessor(Node predecessor);
void update_finger_table(Node s, int i);

/* Remote functions */
Node fetch_successor(Node n);
Node fetch_predecessor(Node n);
Node query_successor(uint32_t key, Node n);
Node query_predecessor(uint32_t key, Node n);
Node query_closest_preceding_finger(uint32_t key, Node n);
void request_update_successor(Node successor, Node n);
void request_update_predecessor(Node predecessor, Node n);
void request_update_finger_table(Node s, int i, Node n);
Node parse_incoming_node(rio_t *client);

/* Utility functions */
uint32_t hash_address(char *ip_address, int port);

Node fetch_query(Node n, char message[]);
void send_request(Node n, char message[]);

void print_node(Node n);
void println();
bool is_equal(Node a, Node b);

Node self_node;
Node self_predecessor;
Node self_successor;
Node self_finger_table[KEY_SIZE];
char self_data[32][MAXLINE]; // Array of keys for simulating <key value> pairs

pthread_mutex_t mutex;

int main(int argc, char *argv[])
{ 
  int listen_port, node_port;

  if (argc == 2) {
    listen_port = atoi(argv[1]);
    initialize_chord(listen_port);
  } else if (argc == 4) {
    listen_port = atoi(argv[1]);
    node_port = atoi(argv[3]);
    join_node(argv[2], node_port, listen_port);
  } else {
    printf("Usage: %s port [node_ip_address node_port]\n", argv[0]);
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

  /* set data to blank */
  int size = sizeof(self_data) / sizeof(self_data[0]);
  for (i = 0; i < size; i++) {
    self_data[i][0] = 0;
  }

  /* SAMPLE data for testing query */
  strcpy(self_data[1], "Gettysburg Address");
  strcpy(self_data[2], "The Art of Computer Programming");

  begin_listening(port);
}

void begin_listening(int port) {
  int listenfd, connfd, optval, clientlen;
  struct sockaddr_in clientaddr;

  listenfd = Open_listenfd(port);
  optval = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
  printf("You are listening on port %d\n", port);
  printf("Your position is %u\n", self_node.key);
  printf("Your predecessor is node %s, port %d, position %u\n", self_predecessor.ip_address, self_predecessor.port, self_predecessor.key);
  printf("Your successor is node %s, port %d, position %u\n", self_successor.ip_address, self_successor.port, self_successor.key);

  pthread_mutex_init(&mutex, NULL);
  while(1) {
    clientlen = sizeof(clientaddr); //struct sockaddr_in

    /* accept a new connection from a client here */
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    printf("Connected to new client, fd: %d\n", connfd);

    int *args = malloc(sizeof(int));
    args[0] = connfd;

    pthread_t thread;
    
    if (pthread_create(&thread, NULL, &receive_client, (void *)args) < 0) {
      printf("receive_client thread error\n");
    }
    
  }
  pthread_mutex_destroy(&mutex);
}

void* receive_client(void *args) {
  int numBytes, clientfd;
  int byteCount = 0;
  char buf1[MAXLINE], buf2[MAXLINE], buf3[MAXLINE];
  rio_t server, client;
  
  clientfd = ((int*)args)[0];
  free(args);

  char request[MAXLINE];
  request[0] = 0;

  printf("Waiting for client request...\n");
  /* Read first line of request */
  Rio_readinitb(&client, clientfd);
  numBytes = Rio_readlineb(&client, request, MAXLINE);
  if (numBytes <= 0) {
    printf("No request received\n");
  }
  printf("Request: %s\n", request);

  // pthread_mutex_lock(&mutex);

  /* Check type of connection */

  /* fetch node's successor */
  if (strncmp(request, "fetch_suc", 9) == 0) {
    printf("Handling fetch_suc\n");
    buf1[0] = 0;
    sprintf(buf1, "%u\n", self_successor.key);
    strcat(buf1, self_successor.ip_address);
    strcat(buf1, "\n");
    sprintf(buf2, "%d\n", self_successor.port);
    strcat(buf1, buf2);
    if (rio_writen(clientfd, buf1, MAXLINE) < 0) {
      perror("Send error:");
    }
    print_node(self_successor);
    printf("%s\n", buf1);

    shutdown(clientfd, SHUT_WR);
    printf("Response sent.\n");
  }  

  /* fetch node's predecessor */
  if (strncmp(request, "fetch_pre", 9) == 0) {
    printf("Handling fetch_pre\n");
    buf1[0] = 0;
    sprintf(buf1, "%u\n", self_predecessor.key);
    strcat(buf1, self_predecessor.ip_address);
    strcat(buf1, "\n");
    sprintf(buf2, "%d\n", self_predecessor.port);
    strcat(buf1, buf2);
    if (rio_writen(clientfd, buf1, MAXLINE) < 0) {
      perror("Send error:");
    }

    printf("Result: \n");
    shutdown(clientfd, SHUT_WR);
    printf("Response sent.\n");
  }

  /* ask node for successor of key */
  if (strncmp(request, "query_suc", 9) == 0) {
    printf("Handling query_suc\n");
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
    printf("Handling query_pre\n");
    uint32_t key = 0;
    key = (uint32_t) atoi(request+9);
    printf("%u\n", key);

    Node predecessor = find_predecessor(key);
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
    printf("Handling query_cpf\n");
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

  /* update node's successor */
  if (strncmp(request, "update_suc", 10) == 0) {
    printf("Handling update_suc\n");

    Node n = parse_incoming_node(&client);
    self_successor = n;
    self_finger_table[0] = n;
    printf("New successor: \n");
    print_node(self_successor);
    Close(clientfd);
    printf("Done update_suc\n");
  }

  /* update node's predecessor */
  if (strncmp(request, "update_pre", 10) == 0) {
    printf("Handling update_pre\n");

    Node n = parse_incoming_node(&client);
    self_predecessor = n;
    printf("New predecessor: \n");
    print_node(self_predecessor);
    Close(clientfd);
    printf("Done update_pre\n");
  }

  /* update node's finger table (entry) */
  if (strncmp(request, "update_fin", 10) == 0) {
    printf("Handling update_fin\n");
    uint32_t index;

    Node s = parse_incoming_node(&client);

    numBytes = Rio_readlineb(&client, request, MAXLINE);
    if (numBytes <= 0) {
      printf("No request received\n");
    } else {
      index = (uint32_t) atoi(request);
    }

    update_finger_table(s, index);

    Close(clientfd);
    printf("Done update_suc\n");
  }

  /* QUERY - ask for data given search_key */
  if (strncmp(request, "search_query", 12) == 0) {
    printf("Handling search_query\n");

    char search_key[MAXLINE], response[MAXLINE];
    strcpy(search_key, request+12);
    int size = sizeof(self_data) / sizeof(self_data[0]);
    bool key_found = false;
    int i;
    for (i = 0; i < size; i++) {
      if (strcmp(self_data[i], search_key) == 0) {
        key_found = true;
      }
    }

    if (key_found) {
      strcpy(response, "Search key found.");
    } else {
      strcpy(response, "Not found.");
    }

    if (rio_writen(clientfd, response, MAXLINE) < 0) {
      perror("Send error:");
    }

    shutdown(clientfd, SHUT_WR);
    printf("Response sent.\n");
  }

  /* Ask for finger table */
  if (strncmp(request, "print_table", 11) == 0) {
    printf("Printing self finger table: \n");
    int i;
    for (i = 0; i < KEY_SIZE; i++) {
      printf("Finger %d: \n", i);
      print_node(self_finger_table[i]);
      println();
    }
    printf("Finished printing finger table.\n");
  }

  // pthread_mutex_unlock(&mutex);
}

Node parse_incoming_node(rio_t *client) {
  int numBytes;
  char request[MAXLINE];
  Node n;

  numBytes = Rio_readlineb(client, request, MAXLINE);
  if (numBytes <= 0) {
    printf("No request received\n");
  } else {
    n.key = (uint32_t) atoi(request);
  }
  request[0] = 0;
  numBytes = Rio_readlineb(client, request, MAXLINE);
  if (numBytes <= 0) {
    printf("No request received\n");
  } else {
    int len = strlen(request);
    if (len > 0 && request[len-1] == '\n') request[len-1] = '\0';
    strcpy(n.ip_address, request);
  }
  request[0] = 0;
  numBytes = Rio_readlineb(client, request, MAXLINE);
  if (numBytes <= 0) {
    printf("No request received\n");
  } else {
    n.port = atoi(request);
  }

  return n;
}

uint32_t hash_address(char *ip_address, int port) {
  char port_str[5];
  unsigned char hash[SHA_DIGEST_LENGTH];
  hash[0] = 0;
  uint32_t key;
  port_str[0] = 0;
  sprintf(port_str, "%d", port);
  char data[strlen(ip_address) + strlen(port_str) + sizeof(char)];
  data[0] = 0;
  strcat(data, ip_address);
  strcat(data, ":");
  strcat(data, port_str);
  SHA1(data, sizeof(data), hash);
  memcpy(&key, hash + 16, sizeof(key));
  return key;
}

/* Join */
void join_node(char *ip_address, int node_port, int listen_port) {
  uint32_t key = 0;
  int serverfd, i;

  /* Set up local node attributes */
  key = hash_address(LOCAL_IP_ADDRESS, listen_port);
  strcpy(self_node.ip_address, LOCAL_IP_ADDRESS);
  self_node.port = listen_port;
  self_node.key = key;

  /* set data to blank */
  int size = sizeof(self_data) / sizeof(self_data[0]);
  for (i = 0; i < size; i++) {
    self_data[i][0] = 0;
  }

  /* Initialize remote note */
  Node fetch_node;
  strcpy(fetch_node.ip_address, ip_address);
  fetch_node.port = node_port;
  fetch_node.key = key;

  /* Initialize predecessor, successor */
  self_successor = query_successor(key, fetch_node);
  self_finger_table[0] = self_successor;
  print_node(self_successor);
  println();
  self_predecessor = fetch_predecessor(self_successor);
  print_node(self_predecessor);
  println();
  request_update_predecessor(self_node, self_predecessor);

  /* Initialize finger table */
  i = 0;
  int product = 1;
  /* n+2^i where i = 0..<m */
  for (i = 1; i < KEY_SIZE; i++) {
    uint32_t start_key = (key + product) % KEY_SPACE;
    if (is_between(start_key, self_node.key, self_finger_table[i-1].key)) {
      self_finger_table[i] = self_finger_table[i-1];
    } else {
      self_finger_table[i] = query_successor(start_key, fetch_node);
    }
    product = product * 2;
    printf("finger %d\n", i);
    print_node(self_finger_table[i]);
    println();
  }

  /* update others */
  product = 1;
  for (i = 0; i < KEY_SIZE; i++) {
    Node p = find_predecessor(self_node.key - product);
    print_node(p);
    request_update_finger_table(self_node, i, p);
    product = product * 2;
  }

  printf("Joining the Chord ring.\n");
  printf("You are listening on port %d\n", self_node.port);
  printf("Your position is %u\n", self_node.key);
  printf("Your predecessor is node %s, port %d, position %u\n", self_predecessor.ip_address, self_predecessor.port, self_predecessor.key);
  printf("Your successor is node %s, port %d, position %u\n", self_successor.ip_address, self_successor.port, self_successor.key);
  begin_listening(listen_port);
}

Node find_successor(uint32_t key) {
  Node n = find_predecessor(key);
  printf("Call fetch successor from find successor\n");
  printf("predecessor: \n");
  print_node(n);
  return fetch_successor(n);
}

Node find_predecessor(uint32_t key) {
  printf("self node %u\n", self_node.key);
  if (self_node.key == self_successor.key) {
    return self_node;
  }
  Node n = self_node;
  Node suc = self_successor;

  printf("Finding pred to %u with node: \n", key);
  print_node(n);
  while (!is_between(key, n.key, suc.key) && key != suc.key) {
    printf("self node %u\n", self_node.key);
    printf("n is: %u\n", n.key);
    printf("suc is: %u\n", suc.key);
    printf("%u is not between %u and %u\n", key, n.key, suc.key);
    printf("self node %u\n", self_node.key);
    printf("Calling cpf on:\n");
    print_node(n);
    Node n_prime = query_closest_preceding_finger(key, n);
    if (is_equal(n, n_prime))
      break;
    n = n_prime;
    printf("Result: \n");
    print_node(n);
    suc = fetch_successor(n);
  }
  return n;
}

Node closest_preceding_finger(uint32_t key) {
  int i;
  for (i = KEY_SIZE - 1; i >= 0; i--) {
    printf("self node %u\n", self_node.key);
    printf("index %d\n", i);
    if (is_between(key, self_finger_table[i].key, self_node.key)) {
      printf("self node %u\n", self_node.key);
      printf("cpfff %u is between %u and %u \n", key, self_finger_table[i].key, self_node.key);
      return self_finger_table[i];
    }
  }
  return self_node;
}

void update_successor(Node successor) {

}

void update_predecessor(Node predecessor) {

}

void update_finger_table(Node s, int i) {
  if (is_between(s.key, self_node.key, self_finger_table[i].key)) {
    self_finger_table[i] = s;
    if (i == 0) {
      self_successor = s;
    }
    printf("Finger for index %d is now: \n", i);
    print_node(s);
    println();
    Node p = self_predecessor;
    if (!is_equal(p, s)) {
      request_update_finger_table(s, i, p);
    }
  }
}

bool is_between(uint32_t key, uint32_t a, uint32_t b) {
  printf("%u key\n", key);
  printf("%u a\n", a);
  printf("%u b\n", b);
  if (a == b) {
    return true;
  }
  if (a < key) {
    printf("a < key\n");
    if (b < a) { // wrap around
      printf("b < a\n");
      return true;
    } else {
      if (key < b) {
        printf("key < b\n");
        return true;
      }
    }
  } else {
    printf("a >= key\n");
    if (key < b && b < a) {
      return true;
    }
  }
  return false;
}

Node fetch_successor(Node n) {
  if (is_equal(n, self_node)) {
    return self_successor;
  }
  print_node(n);
  printf("not equal to self \n");
  print_node(self_node);
  char request_string[MAXLINE];
  request_string[0] = 0;
  strcat(request_string, "fetch_suc");
  printf("calling fetch on first node \n");
  return fetch_query(n, request_string);
}

Node fetch_predecessor(Node n) {
  if (is_equal(n, self_node)) {
    return self_predecessor;
  }
  char request_string[MAXLINE];
  request_string[0] = 0;
  strcat(request_string, "fetch_pre");
  return fetch_query(n, request_string);
}

Node query_predecessor(uint32_t key, Node n) {
  if (is_equal(n, self_node)) {
    return self_predecessor;
  }
  char request_string[MAXLINE];
  request_string[0] = 0;
  strcat(request_string, "query_pre");
  sprintf(request_string+9, "%u", key);
  return fetch_query(n, request_string);
}

Node query_successor(uint32_t key, Node n) {
  if (is_equal(n, self_node)) {
    return self_successor;
  }
  char request_string[MAXLINE];
  request_string[0] = 0;
  strcat(request_string, "query_suc");
  sprintf(request_string+9, "%u", key);
  return fetch_query(n, request_string);
}

Node query_closest_preceding_finger(uint32_t key, Node n) {
  if (is_equal(n, self_node)) {
    return closest_preceding_finger(key);
  }
  char request_string[MAXLINE];
  request_string[0] = 0;
  strcat(request_string, "query_cpf");
  sprintf(request_string+9, "%u", key);
  return fetch_query(n, request_string);
}

void request_update_successor(Node successor, Node n) {
  if (is_equal(n, self_node)) {
    self_successor = successor;
    self_finger_table[0] = successor;
    return;
  }
  char request_string[MAXLINE], buf1[MAXLINE];
  request_string[0] = 0;
  strcat(request_string, "update_suc\n");
  sprintf(buf1, "%u\n", successor.key);
  strcat(request_string, buf1);
  strcat(request_string, successor.ip_address);
  strcat(request_string, "\n");
  sprintf(buf1, "%d\n", successor.port);
  strcat(request_string, buf1);

  send_request(n, request_string);
}

void request_update_predecessor(Node predecessor, Node n) {
  if (is_equal(n, self_node)) {
    printf("NOOO\n");
    self_predecessor = predecessor;
    return;
  }
  char request_string[MAXLINE], buf1[MAXLINE];
  request_string[0] = 0;
  strcat(request_string, "update_pre\n");
  sprintf(buf1, "%u\n", predecessor.key);
  strcat(request_string, buf1);
  strcat(request_string, predecessor.ip_address);
  strcat(request_string, "\n");
  sprintf(buf1, "%d\n", predecessor.port);
  strcat(request_string, buf1);

  send_request(n, request_string);
}

void request_update_finger_table(Node s, int i, Node n) {
  if (is_equal(n, self_node)) {
    self_finger_table[i] = s;
    return;
  }
  char request_string[MAXLINE], buf1[MAXLINE];
  request_string[0] = 0;
  strcat(request_string, "update_fin\n");
  sprintf(buf1, "%u\n", s.key);
  strcat(request_string, buf1);
  strcat(request_string, s.ip_address);
  strcat(request_string, "\n");
  sprintf(buf1, "%d\n", s.port);
  strcat(request_string, buf1);
  sprintf(buf1, "%d\n", i);
  strcat(request_string, buf1);

  send_request(n, request_string);
}

Node fetch_query(Node n, char message[]) {
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
  Close(sock);

  return return_node;
}

void send_request(Node n, char message[]) {
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
}

void print_node(Node n) {
  printf("Key: %u\n", n.key);
  printf("IP: %s\n", n.ip_address);
  printf("port: %d\n", n.port);
}

void println() {
  printf("\n");
}

bool is_equal(Node a, Node b) {
  if (strcmp(a.ip_address, b.ip_address) == 0 && a.port == b.port) {
    return true;
  } 
  return false;
}

