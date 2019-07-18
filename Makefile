default: main

all: main main.html

main: vis.cpp main.cpp
	g++ -O -Wall -I/usr/local/include/SDL2 -std=c++1z -lSDL2 -lSDL2_image -lSDL2_ttf $^ -o $@

main.html: vis.cpp main.cpp
	emcc $^ -g4 -std=c++1z -s USE_SDL=2 -s USE_SDL_TTF=2 -s USE_SDL_IMAGE=2 -s SDL2_IMAGE_FORMATS='["png"]' -o $@ --preload-file data

clean:
	rm -f main main.html main.data main.wasm main.js
