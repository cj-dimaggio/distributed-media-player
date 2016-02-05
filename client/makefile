CFLAGS=-g -Wall
BIN=./bin

client:
	gcc client.c video.c network.c $(CFLAGS) -lavutil -lavformat -lavcodec `sdl2-config --cflags --libs` -o $(BIN)/client
