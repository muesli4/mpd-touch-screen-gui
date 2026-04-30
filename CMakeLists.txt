# SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
#
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.16)

# Use BoostConfig.cmake (provided by Boost itself) rather than the deprecated
# FindBoost module on CMake >= 3.30.
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

project(mpd-touch-screen-gui
    VERSION 0.0.1
    DESCRIPTION "Simple configurable MPD client for (touch and) LCD screens"
    HOMEPAGE_URL "https://github.com/muesli4/mpd-touch-screen-gui"
    LANGUAGES CXX)

include(GNUInstallDirs)

# C++ standard: match the original gnu++17 setting.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# Default to a Release build when invoked without -DCMAKE_BUILD_TYPE=...
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------

find_package(PkgConfig REQUIRED)

pkg_check_modules(SDL2        REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(SDL2_IMG    REQUIRED IMPORTED_TARGET SDL2_image)
pkg_check_modules(SDL2_TTF    REQUIRED IMPORTED_TARGET SDL2_ttf)
pkg_check_modules(LIBWTK_SDL2 REQUIRED IMPORTED_TARGET libwtk-sdl2)
pkg_check_modules(MPD_CLIENT  REQUIRED IMPORTED_TARGET libmpdclient)
pkg_check_modules(ICU_UC      REQUIRED IMPORTED_TARGET icu-uc)
pkg_check_modules(LIBCONFIGPP REQUIRED IMPORTED_TARGET libconfig++)

# Boost::system became header-only in Boost 1.69 and was removed as a
# separately-findable component in Boost 1.90. Request only filesystem; it
# transitively pulls in the system headers target where needed.
find_package(Boost 1.35 REQUIRED COMPONENTS filesystem)
find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# Common install paths
# ---------------------------------------------------------------------------

set(MTSG_PKGDATADIR "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}")
set(MTSG_ICONDIR    "${MTSG_PKGDATADIR}/icons/")

# Definitions formerly auto-provided by Automake (PACKAGE_NAME, VERSION) plus
# the install-path defines previously set via AM_CPPFLAGS in src/Makefile.am.
add_compile_definitions(
    "PACKAGE_NAME=\"${PROJECT_NAME}\""
    "VERSION=\"${PROJECT_VERSION}\""
    "PKGDATA=\"${MTSG_PKGDATADIR}\""
    "ICONDIR=\"${MTSG_ICONDIR}\"")

# ---------------------------------------------------------------------------
# Main GUI binary: mpd-touch-screen-gui
# ---------------------------------------------------------------------------

add_executable(mpd-touch-screen-gui
    src/byte_buffer.cpp
    src/config_file.cpp
    src/cover_view.cpp
    src/dynamic_image_data.cpp
    src/event_loop.cpp
    src/filesystem_cover_provider.cpp
    src/idle_timer.cpp
    src/keypad.cpp
    src/main.cpp
    src/mpd_control.cpp
    src/mpd_cover_provider.cpp
    src/navigation_event.cpp
    src/player_gui.cpp
    src/player_mpd_model.cpp
    src/program_config.cpp
    src/search_view.cpp
    src/text_cover_provider.cpp
    src/udp_control.cpp
    src/user_event.cpp
    src/util.cpp
    src/widget_util.cpp)

target_compile_options(mpd-touch-screen-gui PRIVATE -Wall)

target_link_libraries(mpd-touch-screen-gui PRIVATE
    PkgConfig::SDL2
    PkgConfig::SDL2_IMG
    PkgConfig::LIBWTK_SDL2
    PkgConfig::MPD_CLIENT
    PkgConfig::ICU_UC
    PkgConfig::LIBCONFIGPP
    Boost::filesystem
    Threads::Threads)

# ---------------------------------------------------------------------------
# CLI sender: mpd-touch-screen-gui-send
# ---------------------------------------------------------------------------

add_executable(mpd-touch-screen-gui-send
    src/client.cpp
    src/config_file.cpp)

target_compile_options(mpd-touch-screen-gui-send PRIVATE -Wall)

target_link_libraries(mpd-touch-screen-gui-send PRIVATE
    PkgConfig::LIBCONFIGPP
    Boost::filesystem
    Threads::Threads)

# ---------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------

install(TARGETS mpd-touch-screen-gui mpd-touch-screen-gui-send
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES
        data/program.conf
        data/client.conf
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME})

install(FILES
        data/icons/apps.png
        data/icons/keyboard.png
        data/icons/pause.png
        data/icons/play.png
        data/icons/random_off.png
        data/icons/random_on.png
        data/icons/scroll_up.png
        data/icons/scroll_down.png
        data/icons/jump_to_arrow.png
        data/icons/README.md
        data/icons/LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/icons)

install(FILES README.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR})
