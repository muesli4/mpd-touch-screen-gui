AC_INIT([mpd-touch-screen-gui], [0.0.1], [muesli4@gmail.com])

#AC_CONFIG_SRCDIR([src/config.h.in])
#AC_CONFIG_HEADER(src/config.h)

AC_CONFIG_MACRO_DIRS([m4])
AM_INIT_AUTOMAKE([-Wall foreign])

AC_SUBST([AM_CXXFLAGS], ["-std=c++17 -std=gnu++17 -Wall"])

AC_PROG_CXX

PKG_CHECK_MODULES(SDL2, sdl2)
PKG_CHECK_MODULES(SDL2_TTF, SDL2_ttf)
PKG_CHECK_MODULES(SDL2_IMG, SDL2_image)
PKG_CHECK_MODULES(LIBWTK_SDL2, libwtk-sdl2)
PKG_CHECK_MODULES(MPD_CLIENT, libmpdclient)
PKG_CHECK_MODULES(ICU_UC, icu-uc)
PKG_CHECK_MODULES(CONFIG, libconfig++)

AX_BOOST_BASE([1.35.0],,[AC_MSG_ERROR([boost was not found])])
AX_BOOST_FILESYSTEM
AX_BOOST_SYSTEM


# TODO is there a better way to add support the thread header?
AX_PTHREAD


AC_CONFIG_FILES([Makefile src/Makefile data/Makefile])

#AM_GNU_GETTEXT([external])
#AM_GNU_GETTEXT_VERSION([0.19.7])

AC_OUTPUT
