#include "network.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


typedef struct Connection {
    int                    sockfd;
    struct sockaddr_in     server;
} Connection;



void print_and_exit(const char *msg) {
    perror(msg);
    exit(0);
}

void buffered_recv(int socket, void *buffer, size_t length) {
    /*
      The recv function will not always handle the full length specified,
      it will only save whatever bytes have actually made it over the wire.
      This is just a helper function that buffers until all of the bytes specified
      have been read. Note that this does not return any error or response, it will
      simply log and close the program on an error. Also eleminates flags for
      convinence for our purposes
      (I'm certain this functionality has been implimented before and much better.
      This function should be replaced as soon as I come across where it is.)
    */
    size_t offset = 0;
    while (length > offset) {
        ssize_t read = recv(socket, buffer, length, 0);
        if (read < 0) {
            print_and_exit("Error recieving data");
        }
        offset += read;
    }
}

void download_file(int socket, FILE* file, unsigned long length, int bufferSize) {
    size_t offset = 0;
    char buffer[bufferSize];
    while (length > offset) {
        ssize_t read = recv(socket, &buffer, bufferSize, 0);
        fwrite((void*) &buffer, read, 1, file);
        if (read < 0) {
            print_and_exit("Error recieving data");
        }
        offset += read;
    }
}

Connection* Connection_create(char* host, short port) {

    Connection* conn = (Connection*) calloc(1, sizeof(Connection));

    // Create the inital socket
    conn->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn->sockfd < 0) {
        print_and_exit("Unable to open socket");
    }

    // Convert the given information into a usable structure
    conn->server.sin_family = AF_INET;
    conn->server.sin_port = htons(port);

    // It wouldn't take too much code to resolve a hostname but let's just keep
    // things simple and require an IP address
    conn->server.sin_addr.s_addr = inet_addr(host);

    // Establish the connection through the socket
    if (connect(conn->sockfd , (struct sockaddr *)&conn->server , sizeof(conn->server)) < 0) {
        print_and_exit("Unable to establish connection");
    }

    return conn;
}

void Connection_recvMessage(Connection* conn, Message* message, const char* filename) {
    // Get the sequence number
    buffered_recv(conn->sockfd, &message->seqNum, sizeof(unsigned long));

    // Get the next command
    buffered_recv(conn->sockfd, &message->command, sizeof(unsigned char));

    // If this is an upload than we need to handle it (we don't wan't to leave mid message)
    // and save the file contents to the given filename
    if (message->command == UPLOAD) {
        unsigned long contentLength = 0;
        buffered_recv(conn->sockfd, &contentLength, sizeof(unsigned long));

        printf("Starting Download\n");
        FILE* download = fopen(filename, "wb");
        download_file(conn->sockfd, download, contentLength, 1024);
        printf("Download Finished\n");
        fclose(download);
    } else if (message->command == PLAY || message->command == PAUSE) {
        buffered_recv(conn->sockfd, &message->delay, sizeof(unsigned int));
    } 
}

void Connection_destroy(Connection* conn) {
    close(conn->sockfd);
    free(conn);
}

