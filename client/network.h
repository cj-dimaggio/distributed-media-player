#ifndef NETWORK_H
#define NETWORK_H

typedef enum {UPLOAD, INIT, PLAY, PAUSE, TIME} Command;

typedef struct Connection Connection;

typedef struct Message {
    unsigned long          seqNum;
    Command                command;
    unsigned int           delay; // Note that this is not a relative delay, this is the actual unix timestamp we want to execute at
} Message;

Connection* Connection_create(char* host, short port);
void Connection_destroy(Connection* conn);
void Connection_recvMessage(Connection* conn, Message* message, const char* filename);

#endif
