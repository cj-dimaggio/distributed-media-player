#include <stdlib.h>
#include <stdio.h>

#include "network.h"
#include "video.h"


#define DOWNLOAD_LOCATION "download.mp4"


int main(int argc, char* argv[]) {

    if (argc < 3) {
        fprintf(stderr, "Invalid number of arguments.\n");
        fprintf(stderr, "Usage: %s [hostname] [port] \n", argv[0]);
        exit(0);
    }

    VideoPlayer* videoPlayer = NULL;
    Connection* conn = Connection_create(argv[1], atoi(argv[2]));

    int running = 1;

    while (running) {
        Message message = {0};
        Connection_recvMessage(conn, &message, DOWNLOAD_LOCATION);

        switch(message.command) {
        case INIT:
            videoPlayer = VideoPlayer_create(DOWNLOAD_LOCATION);
            break;
        case PLAY:
            VideoPlayer_play();
            break;
        case PAUSE:
            VideoPlayer_pause();
            break;
        case UPLOAD:
            break;
        case TIME:
            break;
        }
    }

    VideoPlayer_destroy(videoPlayer);
}
