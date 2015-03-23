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
  } else {
    printf("Usage: %s ip_address port\n", argv[0]);
    exit(1);
  }
}

void initialize_query(char *ip_address, int port) {
  Node return_node;
  int sock;
  uint32_t key, hash_value;
  struct sockaddr_in server_addr;
  rio_t server;
  char search_key[MAXLINE];

  if ((sock = socket(AF_INET, SOCK_STREAM/* use tcp */, 0)) < 0) {
    perror("Create socket error:");
  }

  server_addr.sin_addr.s_addr = inet_addr(ip_address);
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);

  if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
    perror("Connect error:");
  }

  key = hash_address(ip_address, port);
  printf("Connected to node %s, port %d, position %u\n", ip_address, port, key);
  printf("Please enter your search key (or type \"quit\" to leave): \n");

  while (1) {
    fflush(stdin);
    gets(search_key);
    if (strncmp(search_key, "quit", 4) == 0) {
      break;
    }

    char request[MAXLINE] = "search_query";

    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(search_key, sizeof(search_key), hash);
    memcpy(&hash_value, hash + 16, sizeof(key));
    printf("Hash value is %x\n", hash_value);

    if (send(sock, search_key, MAXLINE,0) < 0) {
      perror("Send error:");
    }
    // shutdown(sock, SHUT_WR);

    printf("Response from node %s, port %d, position %u\n", ip_address, port, key);
    Rio_readinitb(&server, sock);
    while (Rio_readlineb(&server, request, MAXLINE) > 0) {
      printf("%s\n", request);
    }
  }
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


void print_node(Node n) {
  printf("Key: %u\n", n.key);
  printf("IP: %s\n", n.ip_address);
  printf("port: %d\n", n.port);
}

void println() {
  printf("\n");
}

