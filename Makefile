CXX      = g++

LIBS     = SDL2_ttf SDL2_image sdl2 libmpdclient

CXXFLAGS = '-std=c++11' '-std=gnu++11' '-Wall' `pkg-config --cflags $(LIBS)`
LDFLAGS  = -lpthread `pkg-config --libs $(LIBS)`

OBJ = main.o mpd_control.o util.o timed_thread.o font_atlas.o

main: $(OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJ) -o main

%.o: %.c
	$(CXX) $(CXXFLAGS) -c $<

install: main
	cp main /usr/bin/mpd-touch-screen-gui

replace: main
	systemctl --user stop startx
	sudo cp main /usr/bin/cover
	systemctl --user start startx
