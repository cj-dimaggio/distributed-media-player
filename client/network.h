#ifndef NETWORK_H
#define NETWORK_H

typedef enum {UPLOAD, INIT, PLAY, PAUSE, TIME} Command;

typedef struct Connection Connection;

typedef struct Message {
    unsigned long          seqNum;
    Command                command;
} Message;

Connection* Connection_create(char* host, short port);
void Connection_destroy(Connection* conn);
void Connection_recvMessage(Connection* conn, Message* message, const char* filename);

#endif
