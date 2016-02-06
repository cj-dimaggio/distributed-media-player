// Based heavily on dranger's ffmpeg tutorial at http://dranger.com/ffmpeg/
#include "video.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>


#define QUIT_EVENT (SDL_USEREVENT)
#define PLAY_EVENT (SDL_USEREVENT + 1)
#define PAUSE_EVENT (SDL_USEREVENT + 2)


struct VideoPlayer {
    AVFormatContext*    formatCtx; // Holds information about the entire file
    AVCodecContext*     codecCtx; // Holds information about the particular codec used in the video stream
    AVCodec*            videoCodec; // The actual codec used to decode the video stream

    SDL_mutex*          renderMutex;
    SDL_cond*           renderCond;
    SDL_mutex*           controlMutex;
    SDL_cond*           controlCond;

    int                 videoStream; // The index of the video stream in the file
};


int global_initialized_environment = 0;


void log_and_exit(char* message) {
    fprintf(stderr, "ERROR: %s", message);
    exit(-1);
}

void log_exit_SDL(char* message) {
    fprintf(stderr, "ERROR: %s: %s", message, SDL_GetError());
    exit(-1);
}


int render_thread(void* arg) {
    VideoPlayer* videoPlayer = (VideoPlayer*) arg;

    SDL_LockMutex(videoPlayer->renderMutex);
    // I would prefer to do this in the actual initialization function but it appears
    // there's a bit of weirdness in SDL with initializing a renderer/texture in one
    // thread and then calling them in another. So until there's an overwhelming need
    // I'll just keep the rendering variables local to the thread

    // Create SDL window (This is roughly the SDL2 equivalent of a surface)
    SDL_Window* window = SDL_CreateWindow("Video Player",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          videoPlayer->codecCtx->width,
                                          videoPlayer->codecCtx->height,
                                          SDL_RENDERER_ACCELERATED);

    // Create SDL renderer (Roughly an SDL2 equivelant to an overlay that has GPU acceleration)
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);

    // Another migration to SDL2, this is where we'll actually write the bits to
    SDL_Texture* texture = SDL_CreateTexture(renderer,
                                             SDL_PIXELFORMAT_IYUV,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             videoPlayer->codecCtx->width,
                                             videoPlayer->codecCtx->height);

    int quit = 0;
    int playing = 0;

    // regulating framerate this way is probably incredibly stupid to anybody
    // who knows anything about video encoding and can be quickly confirmed
    // by trying it with the actual framerate given (25 I believe and runs incredibly
    // choppy). So we just hardcode 60 for concept purposes
    //unsigned int framerate = videoPlayer->codecCtx->framerate.num;
    unsigned int framerate = 100;
    unsigned  milliseconds_per_frame = 1000 / framerate;
    Uint32 start_time = 0;
    Uint32 end_time = 0;

    int frameFinished;
    AVPacket packet;
    AVFrame* frame = av_frame_alloc();

    SDL_CondSignal(videoPlayer->renderCond);
    SDL_UnlockMutex(videoPlayer->renderMutex);

    while(!quit) {
        // Event loop
        SDL_Event event;
        SDL_PollEvent(&event);
        switch(event.type) {
        case PLAY_EVENT:
            playing = 1;
            break;
        case PAUSE_EVENT:
            playing = 0;
            break;
        case QUIT_EVENT:
            // Cleanly shutdown the thread
            av_free(frame);

            SDL_DestroyTexture(texture);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);

            quit = 1;

            SDL_CondSignal(videoPlayer->renderCond);
            break;
        default:
            break;
        }

        if (!quit && playing) {
            if(av_read_frame(videoPlayer->formatCtx, &packet) >= 0) {
                // We want to read the start time from the *last* iteration
                if(start_time){
                    end_time = SDL_GetTicks();
                    Uint32 delta = end_time - start_time;
                    if (delta < milliseconds_per_frame){
                        SDL_Delay(milliseconds_per_frame - delta);
                    }
                }
                start_time = SDL_GetTicks();
                // Is this a packet from the video stream?
                if(packet.stream_index == videoPlayer->videoStream) {
                    // Decode video frame
                    avcodec_decode_video2(videoPlayer->codecCtx,frame,
                                          &frameFinished, &packet);

                    // Did we get a video frame?
                    if (frameFinished) {
                        SDL_UpdateYUVTexture(texture, NULL, frame->data[0],
                                             frame->linesize[0], frame->data[1],
                                             frame->linesize[1], frame->data[2],
                                             frame->linesize[2]);

                        SDL_RenderClear(renderer);
                        SDL_RenderCopy(renderer, texture, NULL, NULL);
                        SDL_RenderPresent(renderer);
                    }
                }
                av_free_packet(&packet);
            } else {
                quit = 1;
            }
        }
    }

    return 0;
}

void initialize_video_environment() {
    if (!global_initialized_environment) {
        // Initialize SDL window
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
            log_exit_SDL("Could not initialize SDL context");
        }

        // Register all codecs with ffmpeg
        av_register_all();

        global_initialized_environment = 1;
    }
}

VideoPlayer* VideoPlayer_create(char* filename) {
    initialize_video_environment();

    VideoPlayer* videoPlayer = (VideoPlayer*) calloc(1, sizeof(VideoPlayer));

    // Open the file
    if (avformat_open_input(&videoPlayer->formatCtx, filename, NULL, NULL) != 0) {
        log_and_exit("Unable to open the file");
    }

    // Get stream info from the video file
    if (avformat_find_stream_info(videoPlayer->formatCtx, NULL) < 0) {
        log_and_exit("Unable to get stream info from file");
    }

    // Dump information about file into standard error
    av_dump_format(videoPlayer->formatCtx, 0, filename, 0);

    // Find the first video stream
    int videoStream = -1;
    for (int i = 0; i < videoPlayer->formatCtx->nb_streams; i++) {
        if (videoPlayer->formatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = i;
            break;
        }
    }
    if (videoStream == -1) {
        log_and_exit("Unable to find video stream in file");
    }
    videoPlayer->videoStream = videoStream;

    AVCodecContext* codecCtxOrig = videoPlayer->formatCtx->streams[videoStream]->codec;

    // Get the decoder for the video stream
    videoPlayer->videoCodec = avcodec_find_decoder(codecCtxOrig->codec_id);
    if (videoPlayer->videoCodec == NULL) {
        log_and_exit("Unsupported codec");
    }

    // Copy the context
    videoPlayer->codecCtx = avcodec_alloc_context3(videoPlayer->videoCodec);
    if (avcodec_copy_context(videoPlayer->codecCtx, codecCtxOrig) != 0) {
        log_and_exit("Couldn't copy codec context");
    }

    // Open codec
    if (avcodec_open2(videoPlayer->codecCtx, videoPlayer->videoCodec, NULL) < 0) {
        log_and_exit("Couldn't open codec");
    }

    videoPlayer->renderMutex = SDL_CreateMutex();
    videoPlayer->renderCond = SDL_CreateCond();
    SDL_CreateThread(render_thread, "Player Thread", videoPlayer);

    // Wait until render_thread is ready to start playing
    SDL_LockMutex(videoPlayer->renderMutex);
    SDL_CondWait(videoPlayer->renderCond, videoPlayer->renderMutex);
    SDL_UnlockMutex(videoPlayer->renderMutex);

    return videoPlayer;
}

void VideoPlayer_play() {
    SDL_Event event;
    event.type = PLAY_EVENT;
    SDL_PushEvent(&event);
}

void VideoPlayer_pause() {
    SDL_Event event;
    event.type = PAUSE_EVENT;
    SDL_PushEvent(&event);
}

void delay(int delay) {
    SDL_Delay(delay);
}

void VideoPlayer_destroy(VideoPlayer* videoPlayer) {
    SDL_LockMutex(videoPlayer->renderMutex);
    SDL_Event event;
    event.type = QUIT_EVENT;
    SDL_PushEvent(&event);
    SDL_CondWait(videoPlayer->renderCond, videoPlayer->renderMutex);
    SDL_UnlockMutex(videoPlayer->renderMutex);

    SDL_DestroyMutex(videoPlayer->renderMutex);
    SDL_DestroyCond(videoPlayer->renderCond);

    avcodec_close(videoPlayer->codecCtx);
    avformat_close_input(&videoPlayer->formatCtx);

    free(videoPlayer);
}
