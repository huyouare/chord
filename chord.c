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


/*============================================================
 * function declarations
 *============================================================*/

void initializeChord(int port);
void addNewNode(char *ipAddress, char *port);

int main(int argc, char *argv[])
{ 
  int chordPort;
  unsigned char hash[SHA_DIGEST_LENGTH];

  if (argc == 2) {
    chordPort = atoi(argv[1]);
    printf("Creating new Chord ring...\n");
    initializeChord(chordPort);
    printf("Listening on port %d\n", chordPort);
  } else if (argc == 4) {
    chordPort = atoi(argv[1]);
    printf("Joining the Chord ring...\n");
    addNewNode(argv[2], argv[3]);
    printf("You are listening on port %d\n", chordPort);
    printf("Your position is...\n");
  } else {
    printf("Usage: %s port [nodeAddress nodePort]\n", argv[0]);
    exit(1);
  }
}

void initializeChord(int port) {

}

void addNewNode(char *ipAddress, char *port) {
  char hashData[strlen(ipAddress) + strlen(port) + 1];
  hashData[0] = 0;
  unsigned char hash[SHA_DIGEST_LENGTH];
  printf("Hashdata: %s\n", hashData);
  printf("%s\n", ipAddress);
  strcat(hashData, ipAddress);
  strcat(hashData, ":");
  strcat(hashData, port);
  SHA1(hashData, sizeof(hashData), hash);
  printf("%s\n", hashData);
  printf("%s\n", hash);

  
}

