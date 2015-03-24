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


#define   FILTER_FILE   "query.filter"
#define   LOG_FILE      "query.log"
#define   DEBUG_FILE    "query.debug"
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

void initialize_query(char *ip_address, int port);
void send_query(char search_key[], char *ip_address, int port);
void handle_options(char *ip_address, int port, char *option);

Node fetch_query(Node n, char message[]);
void send_request(Node n, char message[]);

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

void print_node(Node n);
void println();

int main(int argc, char *argv[])
{ 
  int listen_port, node_port;

  if (argc == 3) {
    listen_port = atoi(argv[2]);
    initialize_query(argv[1], listen_port);
  } else if (argc == 4) {
    listen_port = atoi(argv[2]);
    handle_options(argv[1], listen_port, argv[3]);
  }
  else {
    printf("Usage: %s ip_address port [options]\n", argv[0]);
    exit(1);
  }
}

void initialize_query(char *ip_address, int port) {
  Node return_node;
  uint32_t key, hash_value;
  char search_key[MAXLINE];

  key = hash_address(ip_address, port);
  printf("Connected to node %s, port %d, position %x\n", ip_address, port, key);

  while (1) {
    printf("Please enter your search key (or type \"quit\" to leave): \n");
    fflush(stdin);

    search_key[0] = 0;
    gets(search_key);
    if (strncmp(search_key, "quit", 4) == 0) {
      break;
    }

    send_query(search_key, ip_address, port);
  }
}

void handle_options(char *ip_address, int port, char *option) {
  Node return_node;
  uint32_t key, hash_value;
  char search_key[MAXLINE];

  key = hash_address(ip_address, port);
  Node n;
  strcpy(n.ip_address, ip_address);
  n.port = port;
  n.key = key;

  if (strncmp(option, "fetch_suc", 9) == 0) {
    return_node = fetch_successor(n);
    print_node(return_node);
  }
  if (strncmp(option, "fetch_pre", 9) == 0) {
    return_node = fetch_predecessor(n);
    print_node(return_node);
  }
  if (strncmp(option, "print_table", 11) == 0) {
    send_request(n, option);
  }
}

void send_query(char search_key[], char *ip_address, int port) {
  int sock;
  uint32_t key, hash_value;
  struct sockaddr_in server_addr;
  rio_t server;

  if ((sock = socket(AF_INET, SOCK_STREAM/* use tcp */, 0)) < 0) {
    perror("Create socket error:");
  }

  server_addr.sin_addr.s_addr = inet_addr(ip_address);
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);

  if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
    perror("Connect error:");
  }

  char request[MAXLINE] = "search_query";
  strcat(request, search_key);

  unsigned char hash[SHA_DIGEST_LENGTH];
  SHA1(search_key, strlen(search_key), hash);
  memcpy(&hash_value, hash + 16, sizeof(hash_value));
  printf("Hash value is %x\n", hash_value);

  if (send(sock, request, MAXLINE,0) < 0) {
    perror("Send error:");
  }

  key = hash_address(ip_address, port);
  printf("Response from node %s, port %d, position %x\n", ip_address, port, key);
  Rio_readinitb(&server, sock);
  while (Rio_readlineb(&server, request, MAXLINE) > 0) {
    if (request[0] != '\0') {
      printf("%s\n", request);
    }
  }
  Close(sock);
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
  return fetch_query(n, request_string);
}

Node fetch_predecessor(Node n) {
  char request_string[MAXLINE];
  request_string[0] = 0;
  strcat(request_string, "fetch_pre");
  return fetch_query(n, request_string);
}

Node query_predecessor(uint32_t key, Node n) {
  char request_string[MAXLINE];
  request_string[0] = 0;
  strcat(request_string, "query_pre");
  sprintf(request_string+9, "%u", key);
  return fetch_query(n, request_string);
}

Node query_successor(uint32_t key, Node n) {
  char request_string[MAXLINE];
  request_string[0] = 0;
  strcat(request_string, "query_suc");
  sprintf(request_string+9, "%u", key);
  return fetch_query(n, request_string);
}

Node query_closest_preceding_finger(uint32_t key, Node n) {
  char request_string[MAXLINE];
  request_string[0] = 0;
  strcat(request_string, "query_cpf");
  sprintf(request_string+9, "%u", key);
  return fetch_query(n, request_string);
}

void request_update_successor(Node successor, Node n) {
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
  printf("Key: %x\n", n.key);
  printf("IP: %s\n", n.ip_address);
  printf("port: %d\n", n.port);
}

void println() {
  printf("\n");
}

