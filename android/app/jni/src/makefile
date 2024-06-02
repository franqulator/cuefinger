main: main.cpp
	mkdir -p ../build
	mkdir -p ../build/linux
	g++ vector2d.cpp wrapper.cpp translator.cpp misc.cpp gfx2d_collision.cpp gfx2d_fileio.cpp gfx2d_filter.cpp gfx2d_sdl.cpp network_linux.cpp simdjson.cpp main.cpp -lSDL2 -lSDL2main -lSDL2_ttf -o ../build/linux/cuefinger
	chmod +x ../build/linux/cuefinger
