// SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <SDL2/SDL.h>

// mpd_state
#include <mpd/client.h>

#include "cover_view.hpp"
// TODO split into type and sender
#include "navigation_event.hpp"

struct player_view
{
    virtual void on_cover_updated(std::string cover_path) = 0;
    virtual void on_cover_updated(std::string title, std::string artist, std::string album) = 0;

    virtual void on_song_changed(unsigned int new_song_position) = 0;
    virtual void on_playlist_changed(bool reset_position) = 0;
    virtual void on_random_changed(bool random) = 0;
    virtual void on_playback_state_changed(mpd_state playback_state) = 0;

    virtual void on_navigation_event(navigation_event const & ne) = 0;
    virtual void on_other_event(SDL_Event const & e) = 0;
    virtual void on_draw_dirty_event() = 0;
};
