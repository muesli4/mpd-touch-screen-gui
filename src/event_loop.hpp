#pragma once

#include <queue>

#include <SDL2/SDL.h>

#include "program_config.hpp"
#include "navigation_event.hpp"
#include "player_view.hpp"
#include "user_event.hpp"
#include "mpd_control.hpp"
#include "player_mpd_model.hpp"
#include "quit_action.hpp"


bool idle_timer_enabled(program_config const & cfg);

struct event_loop
{
    event_loop(SDL_Renderer * renderer, program_config const & cfg);

    quit_action run(program_config const & cfg);

    private:

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

    mpd_control _mpd_control;

    player_mpd_model _model;

    std::unique_ptr<player_view> _player_view;
};
