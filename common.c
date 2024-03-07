#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"

/*
 * Function used to receive a specified amount of bytes (corresponding to a known message structure)
 * from a TCP connection, at a given memory location; returns number of received bytes.
 */
int recv_all(int sockfd, void *buffer, size_t len) {

  size_t bytes_received = 0;
  size_t bytes_remaining = len;

    while (bytes_remaining) {
        int rc = recv(sockfd, buffer + bytes_received, bytes_remaining, 0);
        DIE(rc < 0, "send");

        if (rc == 0) {
            break;
        }

        bytes_remaining -= rc;
        bytes_received += rc;
    }


    return bytes_received;
}

/*
 * Function used to send a specified amount of bytes (corresponding to a known message structure)
 * through a TCP connection, from a given memory location; returns number of sent bytes.
 */
int send_all(int sockfd, void *buffer, size_t len) {
  size_t bytes_sent = 0;
  size_t bytes_remaining = len;

    while (bytes_remaining) {
        int rc = send(sockfd, buffer + bytes_sent, bytes_remaining, 0);
        DIE(rc < 0, "send");

        if (rc == 0) {
            break;
        }

        bytes_remaining -= rc;
        bytes_sent += rc;
    }

    return bytes_sent;
}