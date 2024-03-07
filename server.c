#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "list.h"

#define MAX_CONNECTIONS 50  // maximum simultaneous TCP connections, used as "listen" call argument

// subscribers and topics stored in the server
list subscribers;
list topics;


/*
 * Function comparing 2 subscribers based on the socket they are connected to.
 */
int equal_socket(void *a, void *b) {
    subscriber *s1 = (subscriber *)a;
    subscriber *s2 = (subscriber *)b;

    return s1->socket == s2->socket;
}

/*
 * Function comparing a the subscriber associated to a subscription and a second subscriber based on
 * the socket they are connected to.
 */
int equal_socket_sub(void *a, void *b) {
    subscription *sub = (subscription *)a;
    subscriber *s = (subscriber *)b;

    return sub->sub->socket == s->socket;
}

/*
 * Function checking if a given subscriber id already exists in the current database; returns a pointer
 * to said subscriber if found, and NULL if not.
 */
subscriber *already_exists(char *id) {
    for (list p = subscribers; p != NULL; p = p->next) {
        subscriber *s = (subscriber *)p->info;
        if (strcmp(s->id, id) == 0) {
            return s;
        }
    }

    return NULL;
}

/*
 * Function removing the subscriber connected to the given socket from the list of subscribers.
 */
void remove_subscriber(int sockfd) {
    for (list p = subscribers; p != NULL; p = p->next) {
        subscriber *s = (subscriber *)p->info;
        if (s->socket == sockfd) {
            remove_from_list(&subscribers, s, equal_socket);
            return;
        }
    }
}

/*
 * Function that gets the updated connection data for the subscriber pointed at by *original from the
 * newly created and not yet registered subscriber entity created by the server, and then removes said
 * subscriber entity (function used when a past user reconnects to the server).
 */
void replace(int sockfd, subscriber *original) {
    for (list p = subscribers; p != NULL; p = p->next) {
        subscriber *s = (subscriber *)p->info;
        if (s->socket == sockfd) {
            memcpy(original->ip, s->ip, sizeof(s->ip));
            original->port = s->port;

            remove_from_list(&subscribers, s, equal_socket);
            return;
        }
    }
}

/*
 * Function that sends any stored messages the subscriber pointed to by *s may have when reconnecting.
 */
void get_stored_messages(subscriber *s) {
    for (list p = s->stored_messages; p != NULL; p = p->next) {
        stored_message *message = (stored_message *)p->info;
        content_header *hdr = &message->hdr;
        send_all(s->socket, hdr, sizeof(content_header));
        send_all(s->socket, message->topic, hdr->topic_len);
        send_all(s->socket, message->payload, hdr->data_len);
    }

    free_list(&s->stored_messages, free);
}

/*
 * Function for registering a new TCP client in the server's database; returns 0 if id is already in use,
 * 1 if succsefull registration occured, -1 in case of any error.
 */
int register_subscriber(int sockfd, char *id) {
    subscriber *s = already_exists(id);

    if (s) {  // given id already exists in the current list of subscribers
        if (s->connected) {  // client with given id is connected
            printf("Client %s already connected.\n", id);
            return 0;
        } else {  // client with given id is reconnecting
            replace(sockfd, s);
            s->connected = 1;
            s->socket = sockfd;
            printf("New client %s connected from %s:%hu.\n", id, s->ip, s->port);
            get_stored_messages(s);  // get any messages missed when disconnected
            return 1;
        }
    }

    // register new client
    for (list p = subscribers; p != NULL; p = p->next) {
        s = (subscriber *)p->info;
        if (s->socket == sockfd) {
            s->connected = 1;
            memcpy(s->id, id, strlen(id) + 1);
            printf("New client %s connected from %s:%hu.\n", id, s->ip, s->port);
            return 1;
        }
    }

    return -1;
}

/*
 * Function marking the subscriber connected to the given socket as disconnected; socket field is set at -1
 * to not be confused with any future connections of different users from the same socket.
 */
void disconnect_subscriber(int sockfd) {
    for (list p = subscribers; p != NULL; p = p->next) {
        subscriber *s = (subscriber *)p->info;
        if (s->socket == sockfd) {
            s->connected = 0;
            s->socket = -1;
            printf("Client %s disconnected.\n", s->id);
            return;
        }
    }
}

/*
 * Function returning a pointer to the subscriber structure corresponding to the clientconnected to the
 * given socket.
 */
subscriber *get_subscriber(int sockfd) {
    for (list p = subscribers; p != NULL; p = p->next) {
        subscriber *s = (subscriber *)p->info;
        if (s->socket == sockfd) {
            return s;
        }
    }

    return NULL;
}

/*
 * Function checking if the subscriber pointed to by *sub is already subscribed to a specific topic
 * identified by its subscription list; returns a pointer to the corresponding subscription structure
 * if found, NULL if not.
 */
subscription *already_subscribed(list subscriptions, subscriber *sub) {
    for (list p = subscriptions; p != NULL; p = p->next) {
        subscription *s = (subscription *)p->info;
        if (strcmp(s->sub->id, sub->id) == 0) {
            return s;
        }
    }

    return NULL;
}

/*
 * Function registering a new subscription in the server's database. 
 */
void register_subscription(int sockfd, char *title, uint8_t sf) {
    // allocate new subscription structure with given info
    subscription *new = (subscription *)calloc(1, sizeof(subscription));
    DIE(new == NULL, "bad alloc");
    new->sub = get_subscriber(sockfd);
    new->sf = sf;

    // check if topic with given title already exists
    for (list p = topics; p != NULL; p = p->next) {
        topic *t = (topic *)p->info;
        if (strcmp(t->title, title) == 0) {
            subscription *existing = already_subscribed(t->subs, new->sub);
            if (existing) {  // if subscriber is already subscribed to the topic, update its sf value
                existing->sf = sf;
                free(new);
            } else {
                insert_in_list(&t->subs, new);  // add new subscrition to subscription list
            }

            return;
        }
    }

    // allocate and add to the topics list a new topic structure if the topic is newly introduced
    topic *new_topic = (topic *)calloc(1, sizeof(topic));
    DIE(new_topic == NULL, "bad alloc");
    memcpy(new_topic->title, title, strlen(title) + 1);
    new_topic->subs = NULL;
    insert_in_list(&new_topic->subs, new);

    insert_in_list(&topics, new_topic);
}

/* 
 * Function to remove the subscription made by the client connected to the given socket from the
 * subscription list of a given topic.
 */
void register_unsubscription(int sockfd, char *title) {
    for (list p = topics; p != NULL; p = p->next) {
        topic *t = (topic *)p->info;
        if (strcmp(t->title, title) == 0) {
            remove_from_list(&t->subs, get_subscriber(sockfd), equal_socket_sub);
            return;
        }
    }
}

/*
 * Function returning the number of relevant bytes in payload (the content of a message received from a
 * UDP client) based on the type of data transmitted.
 */
int get_payload_length(int type, char *payload) {
    switch (type) {
        case 0:
            return sizeof(uint8_t) + sizeof(uint32_t);
        case 1:
            return sizeof(uint16_t);
        case 2:
            return 2 * sizeof(uint8_t) + sizeof(uint32_t);
        case 3:
            return strlen(payload);
    }

    return 0;
}

/*
 * Function used to format a received UDP message as the established format for the TCP messages to clients,
 * and send the newly formed message.
 */
void send_messages(udp_packet received, struct sockaddr_in cli_addr) {
    content_header info;  // create meta data structure for new message
    info.data_len = get_payload_length(received.data_type, received.payload);
    info.topic_len = strlen(received.topic);

    char *ip = inet_ntoa(cli_addr.sin_addr);
    memcpy(info.ip, ip, sizeof(info.ip));
    info.port = ntohs(cli_addr.sin_port);
    info.data_type = received.data_type;

    char *topic_title = received.topic;
    for (list p = topics; p != NULL; p = p->next) {  // find topic given by the received title in the topic list
        topic *t = (topic *)p->info;
        if (strcmp(t->title, topic_title) == 0) {
            for (list q = t->subs; q != NULL; q = q->next) {  // go through all subscriptions of the topic
                subscription *sub = (subscription *)q->info;
                if (sub->sub->connected) {  // if subscriber is connected, send header and relevand payload bytes
                    send_all(sub->sub->socket, &info, sizeof(info));
                    send_all(sub->sub->socket, received.topic, info.topic_len);
                    send_all(sub->sub->socket, received.payload, info.data_len);
                } else if (sub->sf) {  // if subscriber is disconnected but has store-and-forward enabled, 
                                       // allocate and add new message to its stored_messages list
                    stored_message *new = (stored_message *)calloc(1, sizeof(stored_message));
                    DIE(new == NULL, "bad alloc");

                    memcpy(&new->hdr, &info, sizeof(info));
                    memcpy(new->topic, topic_title, info.topic_len);
                    memcpy(new->payload, received.payload, info.data_len);

                    insert_in_list(&sub->sub->stored_messages, new);
                }
            }

            return;
        }
    }
}

/*
 * Function allocating a new "shell" subscriber structure and adding it to the subscriber list when
 * receiving a new TCP connection.
 */
void add_subscriber_structure(int newsockfd, struct sockaddr_in cli_addr) {
    subscriber *new_subscriber = (subscriber *)calloc(1, sizeof(subscriber));
    DIE(new_subscriber == NULL, "bad alloc");
    char *ip = inet_ntoa(cli_addr.sin_addr);  // get subscriber data from connection info
    memcpy(new_subscriber->ip, ip, strlen(ip) + 1);
    new_subscriber->port = ntohs(cli_addr.sin_port);
    new_subscriber->socket = newsockfd;
    new_subscriber->stored_messages = NULL;
    insert_in_list(&subscribers, new_subscriber);
}

/*
 * Function containing the main logic and multiplexing of the server's functionality.
 */
void run_server(int listenfd, int udpfd) {
    struct pollfd poll_fds[MAX_CONNECTIONS + 3];
    int num_fds = 3;  // current number of poll structures 
    int rc;
    request_header received_tcp;
    connect_packet connect;
    subscribe_packet subscribe;
    unsubscribe_packet unsubscribe;
    udp_packet received_udp;
    char buff[256];

    // set socket for listening for TCP connections
    rc = listen(listenfd, MAX_CONNECTIONS);
    DIE(rc < 0, "bad listen");

    // add stdin as first file descriptor to track
    poll_fds[0].fd = 0;
    poll_fds[0].events = POLLIN;

    // add pollfd for listening socket for TCP connections
    poll_fds[1].fd = listenfd;
    poll_fds[1].events = POLLIN;

    // add pollfd for listening socket for UDP connections
    poll_fds[2].fd = udpfd;
    poll_fds[2].events = POLLIN;

    while (1) {  // wait for events
        rc = poll(poll_fds, num_fds, -1);
        DIE(rc < 0, "bad poll");

        for (int i = 0; i < num_fds; i++) {
            if (poll_fds[i].revents & POLLIN) {
                if (poll_fds[i].fd == 0) {  // event from stdin
                    fgets(buff, sizeof(buff), stdin);
                    
                    char *command = strtok(buff, " \n");

                    // if exit is typed from stdin, stop server
                    if (command && strcmp(command, "exit") == 0) {
                        for (int j = 0; j < num_fds; j++) {
                            close(poll_fds[j].fd);
                        }
                        return;
                    }
                } else if (poll_fds[i].fd == listenfd) {  // event from listening socket for TCP connections
                    struct sockaddr_in cli_addr;
                    socklen_t cli_len = sizeof(cli_addr);
                    // accept new connection
                    int newsockfd = accept(listenfd, (struct sockaddr *)&cli_addr, &cli_len);
                    DIE(newsockfd < 0, "accept");

                    // add new socket to pollfd structure
                    poll_fds[num_fds].fd = newsockfd;
                    poll_fds[num_fds].events = POLLIN;
                    num_fds++;

                    add_subscriber_structure(newsockfd, cli_addr);
                } else if (poll_fds[i].fd == udpfd) {  // socket for UDP connections
                    memset(&received_udp, 0, sizeof(received_udp));
                    struct sockaddr_in client_addr;
                    socklen_t clen = sizeof(client_addr);
                    rc = recvfrom(poll_fds[i].fd, &received_udp, sizeof(udp_packet), 0, (struct sockaddr *)&client_addr, &clen);

                    send_messages(received_udp, client_addr);
                } else {  // received data from a TCP connection (subscriber)
                    // receive meta data first (request_header)
                    rc = recv_all(poll_fds[i].fd, &received_tcp, sizeof(received_tcp));
                    DIE(rc < 0, "bad recv");

                    if (rc == 0) {  // if user disconnects, remove its socket from the poll structure
                        close(poll_fds[i].fd);
                        disconnect_subscriber(poll_fds[i].fd);

                        for (int j = i; j < num_fds - 1; j++) {
                            poll_fds[j] = poll_fds[j + 1];
                        }

                        num_fds--;

                    } else {
                        switch (received_tcp.type) {  // proceed according to type of request received
                            case 0:  // receive login request
                                recv_all(poll_fds[i].fd, &connect, received_tcp.len);
                                if (!register_subscriber(poll_fds[i].fd, connect.id)) {
                                    // remove "shell" subscriber structure from subscriber list and close
                                    // the connection if client tried to login with an aready existing active id
                                    close(poll_fds[i].fd);
                                    remove_subscriber(poll_fds[i].fd);

                                    for (int j = i; j < num_fds - 1; j++) {
                                        poll_fds[j] = poll_fds[j + 1];
                                    }

                                    num_fds--;
                                }
                                break;
                            case 1:  // receive subscribe request
                                recv_all(poll_fds[i].fd, &subscribe, received_tcp.len);
                                register_subscription(poll_fds[i].fd, subscribe.topic, subscribe.sf);
                                break;
                            case 2:  // register unsubscribe request
                                recv_all(poll_fds[i].fd, &unsubscribe, received_tcp.len);
                                register_unsubscription(poll_fds[i].fd, unsubscribe.topic);
                                break;
                        }
                    }
                }   
            }
        }
    }
}


/*
 * Function creating a new socket of given type on a given port; returns new socket.
 */
int get_socket(int type, uint16_t port) {
    int fd = socket(AF_INET, type, 0);
    DIE(fd < 0, "bad socket");

    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    // enable socket reuse and disable Nagles's algorithm in the case of a TCP socket
    int enable = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");
    if (type ==SOCK_STREAM) {
        if (setsockopt(fd, SOL_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0)
            perror("setsockopt(TCP_NODELAY) failed");
    }

    // fil in server info
    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // bind socket
    int rc = bind(fd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "bad bind");

    return fd;
}

/*
 * Function for deallocating a subscriber structure.
 */
void free_subscriber(void *p) {
    subscriber *s = (subscriber *)p;
    free_list(&s->stored_messages, free);
}


/*
 * Function for deallocating a topic structure
 */
void free_topic(void *p) {
    topic *t = (topic *)p;
    free_list(&t->subs, free);
}


int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    // check number of arguments
    if (argc != 2) {
        fprintf(stderr, "\n Usage: ./server <port>\n");
        return -1;
    }

    // initialise subscriber and topic lists
    subscribers = NULL;
    topics = NULL;

    // parse port as number
    uint16_t port;
    int rc = sscanf(argv[1], "%hu", &port);
    DIE(rc != 1, "Given port is invalid");

    // create TCP and UDP sockets
    int listenfd = get_socket(SOCK_STREAM, port);
    int udpfd = get_socket(SOCK_DGRAM, port);

    // run server and begin waiting for events
    run_server(listenfd, udpfd);

    // close sockets
    close(listenfd);
    close(udpfd);

    // deallocate lists
    free_list(&subscribers, free_subscriber);
    free_list(&topics, free_topic);

    return 0;
}
