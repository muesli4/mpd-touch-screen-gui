CXX      = g++

LIBS     = SDL2_ttf SDL2_image sdl2 libmpdclient

CXXFLAGS = '-std=c++11' '-std=gnu++11' '-Wall' `pkg-config --cflags $(LIBS)`
LDFLAGS  = `pkg-config --libs $(LIBS)`

OBJ = main.o mpd_control.o util.o

main: $(OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJ) -o main

%.o: %.c
	$(CXX) $(CXXFLAGS) -c $<
