// SPDX-FileCopyrightText: 2022 Doron Behar
// SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "event_loop.hpp"

#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <iterator>
#include <mutex>
#include <queue>
#include <thread>

#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <libwtk-sdl2/box.hpp>
#include <libwtk-sdl2/text_button.hpp>
#include <libwtk-sdl2/texture_button.hpp>
#include <libwtk-sdl2/list_view.hpp>
#include <libwtk-sdl2/notebook.hpp>
#include <libwtk-sdl2/padding.hpp>
#include <libwtk-sdl2/sdl_util.hpp>
#include <libwtk-sdl2/util.hpp>
#include <libwtk-sdl2/widget.hpp>
#include <libwtk-sdl2/widget_context.hpp>

#include <boost/ptr_container/ptr_vector.hpp>

#include "cover_provider.hpp"
#include "filesystem_cover_provider.hpp"
#include "mpd_cover_provider.hpp"
#include "text_cover_provider.hpp"
#include "idle_timer.hpp"
#include "navigation_event.hpp"
#include "udp_control.hpp"
#include "user_event.hpp"
#include "util.hpp"
#include "widget_util.hpp"
#include "enum_texture_button.hpp"
#include "player_view.hpp"
#include "player_gui.hpp"

#include "cover_view.hpp"
#include "search_view.hpp"

// Allow debugging from build directory and avoid errors.
#ifndef ICONDIR
#define ICONDIR "../data/icons/"
#endif

// struct gui_view
// {
//     // e.g., load current cover
//     virtual void on_wake_up() = 0;
// 
//     virtual void on_change_event(change_event_type type) = 0;
// };
// 
// 
// // use MVC
// create models for state that proxies directly to MPD
// views are asked which models they want to listen to (i.e. some context type where they can register themselves

template <typename T, typename EventSender, typename Event>
void push_change_event(EventSender & es, Event e, T & old_val, T const & new_val)
{
    if (old_val != new_val)
    {
        old_val = new_val;
        es.push(e);
    }
}

bool is_input_event(SDL_Event & ev)
{
    return ev.type == SDL_MOUSEBUTTONDOWN ||
           ev.type == SDL_MOUSEBUTTONUP ||
           ev.type == SDL_KEYDOWN ||
           ev.type == SDL_KEYUP ||
           ev.type == SDL_FINGERDOWN ||
           ev.type == SDL_FINGERUP;
}

bool is_duplicate_touch_finger_event(SDL_Event & ev)
{
    bool const duplicate_motion =
        ev.type == SDL_MOUSEMOTION && ev.motion.which == SDL_TOUCH_MOUSEID;
    bool const duplicate_button =
        (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP)
        && ev.button.which == SDL_TOUCH_MOUSEID;
    return duplicate_motion || duplicate_button;
}

/**
 * Should the screen be undimmed by this event?
 */
bool is_undim_event(SDL_Event & ev)
{
    if (ev.type == SDL_WINDOWEVENT)
    {
        uint32_t we = ev.window.event;
        // Mouse moved inside the window.
        return we == SDL_WINDOWEVENT_ENTER;
    }
    // Ignore any down events because the up event will follow later.
    else if (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_FINGERDOWN)
    {
        return false;
    }
    else
    {
        return !is_duplicate_touch_finger_event(ev);
    }
}

/**
 * Refresh the current playlist by destructively updating the current state.
 *
 */
void refresh_current_playlist
    ( std::vector<std::string> & cpl
    , unsigned int & cpv
    , mpd_control & mpdc
    )
{
    playlist_change_info pci = mpdc.get_current_playlist_changes(cpv);
    cpl.resize(pci.new_length);
    cpv = pci.new_version;
    for (auto & p : pci.changed_positions)
    {
        cpl[p.first] = p.second;
    }
}

// TODO refactor
bool idle_timer_enabled(program_config const & cfg)
{
    return cfg.dim_idle_timer.delay.count() != 0;
}

/*
// Types of events that come from a model.
enum class model_event_type
{
    PLAYLIST,
    SONG_LOCATION,
    PLAYBACK_STATUS
};

template <typename Function>
struct callback_handler
{
    std::size_t add(Function f)
    {
        int id =_id_gen;
        _functions.insert(id, f);
        _id_gen++;
        return id;
    }

    void remove(std::size_t id)
    {
        _functions.erase(id);
    }

    template <typename T, typename... Args>
    void emit(Args... args)
    {
        for (auto f : _functions)
            f(args...);
    }

    private:

    std::size_t _id_gen;

    std::unordered_map<std::size_t, Function> _functions;
};

typedef std::function<void(playlist_change_info)> playback_status_function;
typedef std::function<void(std::optional<song_location>)> song_location_function;
typedef std::function<void(status_info)> status_function;

struct model_event_handler : callback_handler<playback_status_function>, callback_handler<song_location_function>, callback_handler<status_function>
{
    void process(model_event_type met);
};
*/

void event_loop::handle_other_event(SDL_Event const & e)
{
    if (_nes.is_event_type(e.type))
    {
        navigation_event ne;
        _nes.read(e, ne);
        _player_view->on_navigation_event(ne);
    }
    else
    {
        _player_view->on_other_event(e);
    }
}

void event_loop::add_user_event(std::function<void()> && f)
{
    std::scoped_lock const lock(_user_event_queue_mutex);
    _user_event_queue.emplace(f);
    _change_event_sender.push();
}

event_loop::event_loop(SDL_Renderer * renderer, program_config const & cfg)
    : _playlist()
    , _current_song_pos(0)
    , _refresh_cover(true)
    , _dimmed(false)

    , _mpd_control(
        [&](std::optional<song_location> opt_sl)
        {
            // TODO probably move, as string copy might not be atomic
            //      another idea is to completely move all song information
            //      into the main thread
            if (opt_sl.has_value())
            {
                auto const & sl = opt_sl.value();
                _current_song_path = sl.path;
                _current_song_pos = sl.pos;
            }

            add_user_event([&]()
            {
                _refresh_cover = true;
                _player_view->on_song_changed(_current_song_pos);
            });
        },
        [&](bool value)
        {
            add_user_event([&, value]()
            {
                _player_view->on_random_changed(value);
            });
        },
        [&]()
        {
            add_user_event([&]()
            {
                if (_dimmed)
                {
                    _current_playlist_needs_refresh = true;
                }
                else
                {
                    refresh_current_playlist(_playlist, _current_playlist_version, _mpd_control);
                    _player_view->on_playlist_changed(_current_song_pos >= _playlist.size());
                }
            });
        },
        [&](mpd_state state)
        {
            add_user_event([&, state]()
            {
                _player_view->on_playback_state_changed(state);
            });
        })
    , _model(_mpd_control)
    , _player_view(std::make_unique<player_gui>(renderer, _model, _playlist, _current_song_pos, cfg))
{
    // TODO not clear why this is needed, something is locking up here otherwise
    add_user_event([&]()
    {
        _player_view->on_random_changed(_mpd_control.get_random());
        _player_view->on_playback_state_changed(_mpd_control.get_state());
    });
}

quit_action event_loop::run(program_config const & cfg)
{
    // Set up user events.
    enum_user_event_sender<idle_timer_event_type> tes;

    boost::ptr_vector<cover_provider> cover_providers;

    // add cover providers
    {
        cover_providers.push_back(new mpd_cover_provider(_mpd_control, MPD_COVER_TYPE_ALBUMART));
        cover_providers.push_back(new mpd_cover_provider(_mpd_control, MPD_COVER_TYPE_READPICTURE));
        if (cfg.cover.opt_directory.has_value())
        {
            cover_providers.push_back(new filesystem_cover_provider(cfg.cover));
        }
        cover_providers.push_back(new text_cover_provider());
    }

    idle_timer_info iti(tes);

    // timer is enabled
    if (idle_timer_enabled(cfg))
    {
        // act as if the timer expired and it should dim now
        tes.push(idle_timer_event_type::IDLE_TIMER_EXPIRED);
    }

    std::thread mpdc_thread(&mpd_control::run, std::ref(_mpd_control));

    std::optional<udp_control> opt_udp_control;
    std::thread udp_thread;
    if (cfg.opt_port.has_value())
    {
        opt_udp_control.emplace(cfg.opt_port.value(), _nes);
        udp_thread = std::thread { &udp_control::run, std::ref(opt_udp_control.value()) };
    }

    try
    {
        // get initial state from mpd
        std::tie(_playlist, _current_playlist_version) = _mpd_control.get_current_playlist();

        // TODO ask mpd state!

        SDL_Event ev;

        // TODO move to MVC

        while (!_model.is_finished() && SDL_WaitEvent(&ev) == 1)
        {
            if (is_quit_event(ev))
            {
                std::cout << "Requested quit" << std::endl;
                _model.quit();
            }
            // most of the events are not required for a standalone fullscreen application
            else if (is_input_event(ev) || _nes.is_event_type(ev.type)
                                        || _change_event_sender.is_event_type(ev.type)
                                        || tes.is_event_type(ev.type)
                                        || ev.type == SDL_WINDOWEVENT
                    )
            {
                // handle asynchronous user events synchronously
                if (_change_event_sender.is_event_type(ev.type))
                {
                    std::scoped_lock const lock(_user_event_queue_mutex);
                    while (!_user_event_queue.empty())
                    {
                        _user_event_queue.front()();
                        _user_event_queue.pop();
                    }

                    if (_dimmed)
                        continue;
                }
                // dim idle timer expired
                else if (tes.is_event_type(ev.type))
                {
                    _dimmed = true;
                    system(cfg.dim_idle_timer.dim_command.c_str());
                    continue;
                }
                else
                {
                    if (idle_timer_enabled(cfg))
                    {
                        if (_dimmed)
                        {
                            if (is_undim_event(ev))
                            {
                                if (_current_playlist_needs_refresh)
                                {
                                    refresh_current_playlist(_playlist, _current_playlist_version, _mpd_control);
                                    _current_playlist_needs_refresh = false;
                                    _player_view->on_playlist_changed(_current_song_pos >= _playlist.size());
                                }
                                // ignore one event, turn on lights
                                _dimmed = false;
                                // TODO run after drawing
                                system(cfg.dim_idle_timer.undim_command.c_str());
                                iti.sync();
                                // TODO refactor into class
                                SDL_AddTimer(std::chrono::milliseconds(cfg.dim_idle_timer.delay).count(), idle_timer_cb, &iti);
                            }
                        }
                        else
                        {
                            iti.signal_user_activity();
                            handle_other_event(ev);
                        }
                    }
                    else
                    {
                        handle_other_event(ev);
                    }
                }

                // Avoid unnecessary I/O on slower devices.
                if (_refresh_cover)
                {
                    for (auto const & p : cover_providers)
                    {
                        if (p.update_cover(*_player_view, *this))
                        {
                            break;
                        }
                    }
                    _refresh_cover = false;
                }

                _player_view->on_draw_dirty_event();
            }
        }
    }
    catch (std::exception const & e)
    {
        std::cerr << e.what() << std::endl;
    }

    _mpd_control.stop();
    mpdc_thread.join();

    if (cfg.opt_port.has_value())
    {
        opt_udp_control.value().stop();
        udp_thread.join();
    }

    return _model.get_quit_action();
}

song_info event_loop::get_song_info() const
{
    song_info result;
    result.title = _mpd_control.get_current_title();
    result.artist = _mpd_control.get_current_artist();
    result.album = _mpd_control.get_current_album();
    return result;
}

std::string event_loop::get_song_path() const
{
    return _current_song_path;
}

