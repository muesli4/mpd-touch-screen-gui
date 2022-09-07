#pragma once

#include <memory>

#include <SDL2/SDL.h>
#include <libwtk-sdl2/widget_context.hpp>
#include <libwtk-sdl2/box.hpp>

#include "cover_view.hpp"
#include "search_view.hpp"
#include "player_view.hpp"
#include "player_model.hpp"
#include "enum_texture_button.hpp"

#include "program_config.hpp"

struct player_gui : player_view
{
    player_gui(SDL_Renderer * renderer, player_model & model, std::vector<std::string> & playlist, unsigned int & current_song_pos, program_config const & cfg);

    void on_cover_updated(std::string cover_path);
    void on_cover_updated(std::string title, std::string artist, std::string album);

    void on_song_changed(unsigned int new_song_position);
    void on_playlist_changed(bool reset_position);
    void on_random_changed(bool random);
    void on_playback_state_changed(mpd_state playback_state);

    void on_navigation_event(navigation_event const & ne);
    void on_other_event(SDL_Event const & e);
    void on_draw_dirty_event();

    private:

    void handle_cover_swipe_direction(swipe_direction dir);

    widget_ptr make_shutdown_view();

    void advance_view();

    SDL_Renderer * _renderer;
    player_model & _model;

    std::shared_ptr<cover_view> _cover_view_ptr;
    std::shared_ptr<list_view> _playlist_view_ptr;
    std::shared_ptr<search_view> _search_view_ptr;
    std::shared_ptr<notebook> _view_ptr;
    std::shared_ptr<enum_texture_button<bool, 2>> _random_button_ptr;
    std::shared_ptr<enum_texture_button<mpd_state, 4>> _play_button_ptr;
    box _main_widget;

    widget_context _ctx;

};

