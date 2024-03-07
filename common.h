#ifndef _COMMON_H
#define _COMMON_H 1

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "list.h"



/*
 * Macro useful for checking possible errors.
 */

#define DIE(assertion, call_description)                                       \
  do {                                                                         \
    if (assertion) {                                                           \
      fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__);                       \
      perror(call_description);                                                \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  } while (0)


#pragma pack(1)

/*
 * Structure representing a subscriber entity within the server's system.
 */
typedef struct {
  char id[11], ip[16];
  uint16_t port;
  int socket, connected;  // server socket the user connected to
                          // connected = 1 - user is active, 0 - user disconnected
  list stored_messages;  // messages from topics the user is subscribed to with store-and-forward
                         // enabled, received while they were disconnected
} subscriber;


/*
 * Structure pairing a subscriber to a topic and its store-and-forward option.
 */
typedef struct {
  uint8_t sf;
  subscriber *sub;
} subscription;

/*
 * Structure keeping titles and subscription lists for a topic. 
 */
typedef struct {
  char title[51];
  list subs;
} topic;


/*
 * The given structure of a message received from the UDP clients.
 */
typedef struct {
  char topic[50];
  uint8_t data_type;
  char payload[1500];
} udp_packet;


/*
 * Message structure for initial login of a TCP client.
 */
typedef struct {
  char id[11];
} connect_packet;


/*
 * Message structure for a subscription request from a TCP client.
 */
typedef struct {
  uint8_t sf;
  char topic[51];
} subscribe_packet;


/*
 * Message structure for a unsubscribe-type request from a TCP client.
 */
typedef struct {
  char topic[51];
} unsubscribe_packet;


/*
 * Structure of meta information about incoming data coming from the TCP clients.
 */ 
typedef struct {
  uint8_t type;  // 0 - subscriber connected, 1 - subscribe, 2 - unsubscribe
  int len;  // number of bytes in the actual message, meaning the number of relevant bytes stored in the
            // above described structures
} request_header;


/*
 * Structure containing meta information about incoming messages sent from the server to TCP clients.
 */
typedef struct {
  int data_len;  // length of message content
  int topic_len;  // length of topic title
  char ip[16];  // ip and port of UDP client sending the message
  uint16_t port;
  uint8_t data_type;  // type of message received
} content_header;


/*
 * Structure representing the meta data and content of a message stored for a disconnected TCP client
 * with store-and-forward enabled for a specific topic.
 */
typedef struct {
  content_header hdr;
  char topic[50];
  char payload[1500];
} stored_message;


#pragma pack(1)

int recv_all(int, void *, size_t);
int send_all(int, void *, size_t);

#endif
