main: src/main.cpp
	mkdir build
	mkdir build/linux
	g++ src/vector2d.cpp src/wrapper.cpp src/translator.cpp src/misc.cpp src/gfx2d_collision.cpp src/gfx2d_fileio.cpp src/gfx2d_filter.cpp src/gfx2d_sdl.cpp src/network_linux.cpp src/simdjson.cpp src/main.cpp -lSDL2 -lSDL2main -lSDL2_ttf -o build/linux/cuefinger
