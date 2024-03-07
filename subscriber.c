#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>

#include "common.h"

/*
 * Function building and sending meta data and content for a login request to the server.
 */ 
void send_id(int sockfd, char *id) {
    request_header connect;
    connect.type = 0;
    connect.len = strlen(id) + 1;
    
    send_all(sockfd, &connect, sizeof(connect));

    connect_packet data;
    memcpy(data.id, id, strlen(id) + 1);
    send_all(sockfd, &data, connect.len);
}

/*
 * Function building and sending meta data and content for a subscribe request to the server.
 */ 
void subscribe(int sockfd, char *topic, uint8_t sf) {
    request_header subscribe;
    subscribe.type = 1;
    subscribe.len = strlen(topic) + 2;

    send_all(sockfd, &subscribe, sizeof(subscribe));

    subscribe_packet data;
    memcpy(data.topic, topic, strlen(topic) + 1);
    data.sf = sf;

    send_all(sockfd, &data, subscribe.len);
}

/*
 * Function building and sending meta data and content for a unsubscribe request to the server.
 */ 
void unsubscribe(int sockfd, char *topic) {
    request_header unsubscribe;
    unsubscribe.type = 2;
    unsubscribe.len = strlen(topic) + 1;

    send_all(sockfd, &unsubscribe, sizeof(unsubscribe));

    unsubscribe_packet data;
    memcpy(data.topic, topic, strlen(topic) + 1);

    send_all(sockfd, &data, unsubscribe.len);
}

/*
 * Function returning the corresponding human-readable string representation of a data type received. 
 */ 
char *get_type(uint8_t type) {
    switch(type) {
        case 0:
            return "INT";
        case 1:
            return "SHORT_REAL";
        case 2:
            return "FLOAT";
        case 3:
            return "STRING";
    }

    return "UNKNOWN";
}

/*
 * Function parsing and formatting an "INT" type message payload. 
 */
void print_int(char *payload) {
    uint8_t sign;
    uint32_t value;

    memcpy(&sign, payload, sizeof(sign));
    memcpy(&value, payload + sizeof(sign), sizeof(value));

    if (sign) {
        printf("-%u\n", ntohl(value));
    } else {
        printf("%u\n", ntohl(value));
    }
}

/*
 * Function parsing and formatting a "SHORT_REAL" type message payload. 
 */
void print_short_real(char *payload) {
    uint16_t value;

    memcpy(&value, payload, sizeof(value));

    int integ = ntohs(value) / 100, fract = ntohs(value) % 100;
    
    printf("%d", integ);
    if (fract) {
        if (fract< 10) {
            printf(".0%d\n", fract);
        } else {
            printf(".%d\n", fract);
        }
    } else {
        printf("\n");
    }
}

/*
 * Function parsing and formatting a "FLOAT" type message payload. 
 */
void print_float(char *payload) {
    uint8_t sign;
    uint32_t value;
    uint8_t power;

    memcpy(&sign, payload, sizeof(sign));
    memcpy(&value, payload + sizeof(sign), sizeof(value));
    memcpy(&power, payload + sizeof(sign) + sizeof(value), sizeof(power));

    int pow = 1;
    for (uint8_t i = 0; i < power; i++) {
        pow *= 10;
    }

    int integ = ntohl(value) / pow, fract = ntohl(value) % pow;
    if (sign)
        printf("-");
    
    printf("%d", integ);
    if (fract) {
        printf(".");
        for (int i = pow / 10; i >= 1; i /= 10) {  // print aditional zeros at the beggining of the fractional part
            if (fract < i) {
                printf("0");
            } else {
                break;
            }
        }
        printf("%d\n", fract);
    } else {
        printf("\n");
    }
}

/*
 * Function deciding the appropiate way (function) to interpret the given message payload.
 */
void print_data(int type, char *payload) {
    switch (type) {
        case 0:
            print_int(payload);
            break;
        case 1:
            print_short_real(payload);
            break;
        case 2:
            print_float(payload);
            break;
        case 3:
            printf("%s\n", payload);
            break;
    }
}

/*
 * Function that receives the bytes corresponding to a message from the server over the TCP connection
 * made through "sockfd". 
 */
int recv_content(int sockfd) {
    content_header info;

    int rc = recv_all(sockfd, &info, sizeof(info));  // receive meta data first
    DIE(rc < 0, "bad recv");

    if (rc == 0)
        return 0;

    // receive the indicated number of bytes for each part of the message (title and content)
    char topic[info.topic_len + 1], payload[info.data_len + 1];
    rc = recv_all(sockfd, topic, info.topic_len);
    DIE(rc < 0, "bad recv");
    rc = recv_all(sockfd, payload, info.data_len);
    DIE(rc < 0, "bad recv");

    // adding string terminator in case the topic or the payload contained the maximum allowed number
    // of characters and don't include it already
    topic[info.topic_len] = '\0';
    payload[info.data_len] = '\0';
    printf("%s:%hu - %s - %s - ", info.ip, info.port, topic, get_type(info.data_type));
    print_data(info.data_type, payload);

    return 1;
}


/*
 * Function multiplexing a subscriber's connections to the server and stdin. 
 */
void run_subscriber(int sockfd, char *id) {
    struct pollfd poll_fds[2];
    int rc;
    send_id(sockfd, id);  // first send login request to the server

    // initialising 2 poll structures waiting for events at stdin or the TCP socket
    poll_fds[0].fd = 0;
    poll_fds[0].events = POLLIN;

    poll_fds[1].fd = sockfd;
    poll_fds[1].events = POLLIN;

    char buff[256];
    while (1) {
        rc = poll(poll_fds, 2, -1);
        DIE(rc < 0, "bad poll");

        if (poll_fds[0].revents & POLLIN) {  // event came from stdin
            fgets(buff, sizeof(buff), stdin);

            if (strlen(buff) > 1) {  // check if line read is not empty
                  char *command = strtok(buff, " \n");  // get first word of input

                if (strcmp(command, "exit") == 0) {
                    return;
                }

                if (strcmp(command, "subscribe") == 0) {
                    char *topic = strtok(NULL, " \n");
                    char *sf = strtok(NULL, " \n");
                    
                    if (topic && sf) {  // check if the correct arguments exist
                        if (atoi(sf) != 0 && atoi(sf) != 1) {
                            fprintf(stderr, "SF argument needs to be 0 or 1.\n");
                        }
                        subscribe(sockfd, topic, atoi(sf));
                        printf("Subscribed to topic.\n");
                    }
                }

                if (strcmp(command, "unsubscribe") == 0) {
                    char *topic = strtok(NULL, " \n");

                    if (topic) {
                        unsubscribe(sockfd, topic);
                        printf("Unsubscribed from topic.\n");
                    }
                }   
            }
        }

        if (poll_fds[1].revents & POLLIN) {  // event came from TCP connection
            rc = recv_content(sockfd);  // receive message
            DIE(rc < 0, "bad recv");

            if (rc == 0) {  // connection closes when the server is down
                return;
            }
        }
    }
}


int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    // check given arguments
    if (argc != 4) {
        fprintf(stderr, "\n Usage: ./subscriber <id> <ip> <port>\n");
        return 1;
    }

    char *id = argv[1];
    if (strlen(id) > 10) {
        fprintf(stderr, "User IDs can have at most 10 characters\n");
        return 1;
    }

    // parse port as a number
    uint16_t port;
    int rc = sscanf(argv[3], "%hu", &port);
    DIE(rc != 1, "Given port is invalid");

    // create TCP socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "socket");

    
    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    // enable reuse and deactivate Nagle algorithm
    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");
    if (setsockopt(sockfd, SOL_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0)
            perror("setsockopt(TCP_NODELAY) failed");

    // fill in server information
    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
    DIE(rc <= 0, "inet_pton");

    // connect to server
    rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "connect");

    // run subscriber and begin waiting for events
    run_subscriber(sockfd, id);

    // close socket
    close(sockfd);

    return 0;
}