// SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <queue>

#include <boost/ptr_container/ptr_vector.hpp>

#include <SDL2/SDL.h>

#include "cover_provider.hpp"
#include "song_data_provider.hpp"
#include "program_config.hpp"
#include "navigation_event.hpp"
#include "player_view.hpp"
#include "user_event.hpp"
#include "mpd_control.hpp"
#include "player_mpd_model.hpp"
#include "quit_action.hpp"


bool idle_timer_enabled(program_config const & cfg);

struct event_loop : private song_data_provider
{
    event_loop(SDL_Renderer * renderer, program_config const & cfg);

    quit_action run(program_config const & cfg);

    private:

    // get information about the current song
    song_info get_song_info() const;
    std::string get_song_path() const;

    void fill_cover_providers_from_config(cover_config const & cfg, boost::ptr_vector<cover_provider> & cover_providers);

    void handle_other_event(SDL_Event const & e);

    navigation_event_sender _nes;
    simple_event_sender _change_event_sender;

    std::queue<std::function<void()>> _user_event_queue;
    std::mutex _user_event_queue_mutex;

    void add_user_event(std::function<void()> && f);

    // loop state
    std::vector<std::string> _playlist;
    unsigned int _current_song_pos;
    std::string _current_song_path;
    unsigned int _current_playlist_version;
    bool _refresh_cover;
    bool _dimmed;
    bool _current_playlist_needs_refresh = true;

    // not considered as part of the state since just getting data is typically
    // not possible in const functions
    mutable mpd_control _mpd_control;

    player_mpd_model _model;

    std::shared_ptr<player_view> _player_view;
};
