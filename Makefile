CXX      = g++

LIBS     = SDL2_ttf SDL2_image sdl2 libmpdclient icu-uc libconfig++ libwtk-sdl2

CXXFLAGS = '-std=c++17' '-std=gnu++17' '-Wall' `pkg-config --cflags $(LIBS)`
LDFLAGS  = -lpthread `pkg-config --libs $(LIBS)` -lboost_filesystem -lboost_system

OBJ      = main.o mpd_control.o util.o font_atlas.o sdl_util.o program_config.o idle_timer.o user_event.o keypad.o song_list_view.o search_view.o cover_view.o

debug: CXXFLAGS += '-ggdb' '-O0'
opt:   CXXFLAGS += '-O2'

main: $(OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJ) -o main

debug: main

opt: main

%.o: %.c
	$(CXX) $(CXXFLAGS) -c $<

install: main
	cp main /usr/bin/mpd-touch-screen-gui

replace: main
	systemctl --user stop startx
	sudo cp main /usr/local/bin/cover
	systemctl --user start startx

clean:
	rm *.o
	rm main
