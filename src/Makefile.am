# SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# TODO add those flags
#debug: CXXFLAGS += '-ggdb' '-O0'
#opt:   CXXFLAGS += '-O2'

AM_CPPFLAGS = -DPKGDATA="\"$(pkgdatadir)\"" $(BOOST_CPPFLAGS) -DICONDIR='"$(pkgdatadir)/icons/"'

bin_PROGRAMS = mpd-touch-screen-gui mpd-touch-screen-gui-send

mpd_touch_screen_gui_SOURCES =  \
	byte_buffer.cpp               \
	config_file.cpp               \
	cover_view.cpp                \
	dynamic_image_data.cpp        \
	event_loop.cpp                \
	filesystem_cover_provider.cpp \
	idle_timer.cpp                \
	keypad.cpp                    \
	main.cpp                      \
	mpd_control.cpp               \
	mpd_cover_provider.cpp        \
	navigation_event.cpp          \
	player_gui.cpp                \
	player_mpd_model.cpp          \
	program_config.cpp            \
	search_view.cpp               \
	text_cover_provider.cpp       \
	udp_control.cpp               \
	user_event.cpp                \
	util.cpp                      \
	widget_util.cpp

mpd_touch_screen_gui_LDADD = $(SDL2_LIBS) $(SDL2_IMG_LIBS) $(LIBWTK_SDL2_LIBS) $(MPD_CLIENT_LIBS) $(ICU_UC_LIBS) $(CONFIG_LIBS) $(BOOST_FILESYSTEM_LIB) $(BOOST_SYSTEM_LIB) $(PTHREAD_LIBS) $(PTHREAD_CFLAGS)

mpd_touch_screen_gui_CXXFLAGS = $(SDL2_CFLAGS) $(SDL2_IMG_CFLAGS) $(LIBWTK_SDL2_CFLAGS) $(MPD_CLIENT_CFLAGS) $(ICU_UC_CFLAGS) $(CONFIG_CFLAGS) $(PTHREAD_CFLAGS) @AM_CXXFLAGS@


mpd_touch_screen_gui_send_SOURCES = client.cpp config_file.cpp

mpd_touch_screen_gui_send_LDADD = $(CONFIG_LIBS) $(BOOST_FILESYSTEM_LIB) $(BOOST_SYSTEM_LIB) $(PTHREAD_LIBS) $(PTHREAD_CFLAGS)
mpd_touch_screen_gui_send_CXXFLAGS = $(CONFIG_CFLAGS) $(PTHREAD_CFLAGS) @AM_CXXFLAGS@
