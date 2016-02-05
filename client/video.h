#ifndef VIDEO_H
#define VIDEO_H

typedef struct VideoPlayer VideoPlayer;

VideoPlayer* VideoPlayer_create(char* filename);
void VideoPlayer_destroy(VideoPlayer* videoPlayer);
void VideoPlayer_play();
void VideoPlayer_pause();
void delay(int delay);

#endif
