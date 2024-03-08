# Notice the bin_ prefix.
bin_PROGRAMS = cuefinger

cuefinger_SOURCES = src/vector2d.cpp src/wrapper.cpp src/translator.cpp src/misc.cpp src/gfx2d_collision.cpp \
src/gfx2d_fileio.cpp src/gfx2d_filter.cpp src/gfx2d_sdl.cpp src/network_linux.cpp src/simdjson.cpp src/main.cpp

cuefinger_LDADD = -lSDL2 -lSDL2main -lSDL2_ttf

#cuefinger_CPPFLAGS = $(AM_CPPFLAGS)

dist_data_DATA = build/data/*