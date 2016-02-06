#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "network.h"
#include "video.h"


#define DOWNLOAD_LOCATION "/tmp/download.mp4"


void wait_until(unsigned int delay) {
    struct timespec ts = {delay, 0};
    clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &ts, NULL);
}

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
            wait_until(message.delay);
            VideoPlayer_play();
            break;
        case PAUSE:
            wait_until(message.delay);
            VideoPlayer_pause();
            break;
        case UPLOAD:
            break;
        case TIME:
            // Print the time
            wait_until(message.delay);
            struct timespec ts = {0};
            clock_gettime(CLOCK_REALTIME, &ts);
            printf("Seconds: %ld\nNano Seconds: %ld\n", ts.tv_sec, ts.tv_nsec);
            break;
        }
    }

    VideoPlayer_destroy(videoPlayer);
}
