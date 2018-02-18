# TODO add those flags
#debug: CXXFLAGS += '-ggdb' '-O0'
#opt:   CXXFLAGS += '-O2'

AM_CPPFLAGS = -DPKGDATA="\"$(pkgdatadir)\"" $(BOOST_CPPFLAGS)

bin_PROGRAMS = mpd-touch-screen-gui

mpd_touch_screen_gui_SOURCES = \
	cover_view.cpp               \
	idle_timer.cpp               \
	keypad.cpp                   \
	main.cpp                     \
	mpd_control.cpp              \
	program_config.cpp           \
	search_view.cpp              \
	song_list_view.cpp           \
	user_event.cpp               \
	util.cpp

mpd_touch_screen_gui_LDADD = $(SDL2_LIBS) $(SDL2_IMG_LIBS) $(LIBWTK_SDL2_LIBS) $(MPD_CLIENT_LIBS) $(ICU_UC_LIBS) $(CONFIG_LIBS) $(BOOST_FILESYSTEM_LIB) $(BOOST_SYSTEM_LIB) $(PTHREAD_LIBS) $(PTHREAD_CFLAGS)

mpd_touch_screen_gui_CXXFLAGS = $(SDL2_CFLAGS) $(SDL2_IMG_CFLAGS) $(LIBWTK_SDL2_CFLAGS) $(MPD_CLIENT_CFLAGS) $(ICU_UC_CFLAGS) $(CONFIG_CFLAGS) $(PTHREAD_CFLAGS) @AM_CXXFLAGS@

