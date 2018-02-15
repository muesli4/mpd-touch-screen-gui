// poor mans mpd cover display (used for ILI9341 320x240 pixel display)

#include <iostream>
#include <cstring>
#include <functional>
#include <queue>
#include <array>

#include <iterator>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include <cstdlib>

#include <boost/filesystem.hpp>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include "mpd_control.hpp"
#include "program_config.hpp"
#include "idle_timer.hpp"
#include "user_event.hpp"

//#include "util.hpp"
//#include "sdl_util.hpp"
//#include "font_atlas.hpp"
//#include "gui.hpp"

#include <libwtk-sdl2/widget.hpp>
#include <libwtk-sdl2/button.hpp>
#include <libwtk-sdl2/box.hpp>
#include <libwtk-sdl2/list_view.hpp>
#include <libwtk-sdl2/notebook.hpp>
#include <libwtk-sdl2/widget_context.hpp>
#include <libwtk-sdl2/padding.hpp>
#include <libwtk-sdl2/sdl_util.hpp>
#include <libwtk-sdl2/util.hpp>

#include "cover_view.hpp"
#include "search_view.hpp"
#include "song_list_view.hpp"


// future feature list and ideas:
// TODO replace cycling with a menu
// TODO playlist view: use several tags and display in different cells:
//          - use a header colum (expand when clicked)
//          - scroll with swipe
// TODO playlist view: jump to song position on new song
// TODO move constants into config
// TODO cover view: show a popup when an action has been executed (use timer to refresh)
//                  this may also be used for a remote control, if a popup exists draw it
//                  over everything, remove the popup with a timer by simply sending a refresh event

// determines the minimum length of a swipe
unsigned int const SWIPE_THRESHOLD_LOW_X = 30;
unsigned int const SWIPE_THRESHOLD_LOW_Y = SWIPE_THRESHOLD_LOW_X;

// the time to wait after a swipe, before allowing touch events
std::chrono::milliseconds const SWIPE_WAIT_DEBOUNCE_THRESHOLD(400);

// determines how long a swipe is still recognized as a touch
unsigned int const TOUCH_DISTANCE_THRESHOLD_HIGH = 10;


std::optional<std::string> find_cover_file(std::string rel_song_dir_path, std::string base_dir, std::vector<std::string> names, std::vector<std::string> extensions)
{
    std::string const abs_cover_dir = absolute_cover_path(base_dir, basename(rel_song_dir_path));
    for (auto const & name : names)
    {
        for (auto const & ext : extensions)
        {
            std::string const cover_path = abs_cover_dir + name + "." + ext;
            if (boost::filesystem::exists(boost::filesystem::path(cover_path)))
            {
                return cover_path;
            }
        }
    }

    return std::nullopt;
}

void handle_cover_swipe_action(swipe_action a, mpd_control & mpdc, unsigned int volume_step)
{
    switch (a)
    {
        case swipe_action::UP:
            mpdc.inc_volume(volume_step);
            break;
        case swipe_action::DOWN:
            mpdc.dec_volume(volume_step);
            break;
        case swipe_action::RIGHT:
            mpdc.next_song();
            break;
        case swipe_action::LEFT:
            mpdc.prev_song();
            break;
        default:
            break;
    }
}

enum class view_type
{
    COVER_SWIPE,
    QUEUE,
    SONG_SEARCH,
    SHUTDOWN
};

view_type cycle_view_type(view_type v)
{
    return static_cast<view_type>((static_cast<unsigned int>(v) + 1) % 4);
}

enum quit_action
{
    SHUTDOWN,
    REBOOT,
    NONE
};

template <typename T> void push_change_event(uint32_t event_type, user_event ue, T & old_val, T const & new_val)
{
    if (old_val != new_val)
    {
        old_val = new_val;
        push_user_event(event_type, ue);
    }
}

bool is_quit_event(SDL_Event const & ev)
{
    return ev.type == SDL_QUIT
           || (ev.type == SDL_KEYDOWN && ev.key.keysym.mod & KMOD_CTRL && ev.key.keysym.sym == SDLK_q)
           ;

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

void refresh_current_playlist(std::vector<std::string> & cpl, unsigned int & cpv, mpd_control & mpdc)
{
    playlist_change_info pci = mpdc.get_current_playlist_changes(cpv);
    cpl.resize(pci.new_length);
    cpv = pci.new_version;
    for (auto & p : pci.changed_positions)
    {
        cpl[p.first] = p.second;
    }
}

bool idle_timer_enabled(program_config const & cfg)
{
    return cfg.dim_idle_timer.delay.count() != 0;
}

widget_ptr make_shutdown_view(quit_action & result, bool & run)
{
    return pad(10, vbox({ { false, std::make_shared<button>("Shutdown", [&run, &result](){ result = quit_action::SHUTDOWN; run = false; }) }
                , { false, std::make_shared<button>("Reboot", [&run, &result](){ result = quit_action::REBOOT; run = false; }) }
                }, 5, true));
}

std::string random_label(bool random)
{
    return random ? "¬R" : "R";
}

quit_action program(program_config const & cfg)
{
    quit_action result = quit_action::NONE;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS
                            | (idle_timer_enabled(cfg) ? SDL_INIT_TIMER : 0)
            );
    std::atexit(SDL_Quit);

    // determine screen size of display 0
    SDL_DisplayMode mode = { SDL_PIXELFORMAT_UNKNOWN, 0, 0, 0, 0 };
    if (SDL_GetDisplayMode(0, 0, &mode) != 0)
    {
        std::cerr << "Could not determine screen size:"
                  << SDL_GetError() << '.' << std::endl;
        std::exit(0);
    }

    // Pick maximum resolution in fullscreen.
    if (!cfg.display.fullscreen)
    {
        mode.w = std::min(cfg.display.resolution.w, mode.w);
        mode.h = std::min(cfg.display.resolution.h, mode.h);
    }

    // font rendering setup
    if (TTF_Init() == -1)
    {
        std::cerr << "Could not initialize font rendering:"
                  << TTF_GetError() << '.' << std::endl;
        std::exit(0);
    }

    SDL_Window * window = SDL_CreateWindow
        ( "mpc-touch-lcd-gui"
        , SDL_WINDOWPOS_UNDEFINED
        , SDL_WINDOWPOS_UNDEFINED
        , mode.w
        , mode.h
        , cfg.display.fullscreen ? SDL_WINDOW_FULLSCREEN : 0
        );
    {
        // loop state
        std::string current_song_path;
        bool current_song_exists;

        bool random;
        view_type current_view = view_type::COVER_SWIPE;

        bool refresh_cover = true;

        unsigned int current_song_pos = 0;
        std::vector<std::string> cpl;
        unsigned int cpv;
        bool current_playlist_needs_refresh = false;

        uint32_t const user_event_type = SDL_RegisterEvents(1);
        if (user_event_type == static_cast<uint32_t>(-1))
        {
            throw std::runtime_error("out of SDL user events");
        }
        idle_timer_info iti(user_event_type);

        bool dimmed = false;

        // timer is enabled
        if (idle_timer_enabled(cfg))
        {
            // act as if the timer expired and it should dim now
            push_user_event(user_event_type, user_event::TIMER_EXPIRED);
        }

        mpd_control mpdc(
            [&](song_change_info sci)
            {
                // TODO probably move, as string copy might not be atomic
                //      another idea is to completely move all song information
                //      into the main thread
                current_song_exists = sci.valid;
                if (sci.valid)
                {
                    current_song_path = sci.path;
                    current_song_pos = sci.pos;
                }
                push_user_event(user_event_type, user_event::SONG_CHANGED);
            },
            [&](bool value)
            {
                push_change_event(user_event_type, user_event::RANDOM_CHANGED, random, value);
            },
            [&]()
            {
                push_user_event(user_event_type, user_event::PLAYLIST_CHANGED);
            }
        );

        std::thread mpdc_thread(&mpd_control::run, std::ref(mpdc));

        // get initial state from mpd
        std::tie(cpl, cpv) = mpdc.get_current_playlist();
        random = mpdc.get_random();

        bool run = true;
        SDL_Event ev;

        // TODO move to MVC
        auto random_button = std::make_shared<button>(random_label(random), [&](){ mpdc.set_random(!random); });

        auto cv = std::make_shared<cover_view>([&](swipe_action a){ handle_cover_swipe_action(a, mpdc, 5); }, [&](){ mpdc.toggle_pause(); });
        auto slv = std::make_shared<list_view>(cpl, current_song_pos, [&mpdc](std::size_t pos){ mpdc.play_position(pos); });

        auto view_box = std::make_shared<notebook>(std::vector<widget_ptr>{ cv
                                                   , song_list_view(slv, "Jump", [=, &current_song_pos](){ slv->set_position(current_song_pos); })
                                                   , std::make_shared<search_view>(cfg.on_screen_keyboard.size, cfg.on_screen_keyboard.keys, cpl, [&](auto pos){ mpdc.play_position(pos); })
                                                   , make_shutdown_view(result, run)
                                                   });

        // TODO introduce image button and add symbols from, e.g.: https://material.io/icons/
        auto button_controls = vbox(
                { { false, std::make_shared<button>("♫", [&](){ current_view = cycle_view_type(current_view); view_box->set_page(static_cast<int>(current_view));  }) }
                , { false, std::make_shared<button>("►", [&](){ mpdc.toggle_pause(); }) } // choose one of "❚❚"  "▍▍""▋▋"
                , { false, random_button }
                }, 5);

        box main_widget(box::orientation::HORIZONTAL
                       , { { false, pad(5, button_controls) }
                         , { true, pad(5, view_box) }
                         }, 0, false);

        SDL_Renderer * renderer = renderer_from_window(window);
        widget_context ctx(renderer, { cfg.font_path, 15 }, main_widget);
        ctx.draw();

        while (run && SDL_WaitEvent(&ev) == 1)
        {
            if (is_quit_event(ev))
            {
                std::cout << "Requested quit" << std::endl;
                run = false;
            }
            // most of the events are not required for a standalone fullscreen application
            else if (is_input_event(ev) || ev.type == user_event_type
                                        || ev.type == SDL_WINDOWEVENT
                    )
            {
                // handle asynchronous user events synchronously
                if (ev.type == user_event_type)
                {
                    switch (static_cast<user_event>(ev.user.code))
                    {
                        case user_event::SONG_CHANGED:
                            refresh_cover = true;
                            break;
                        case user_event::PLAYLIST_CHANGED:
                            if (dimmed)
                                current_playlist_needs_refresh = true;
                            else
                            {
                                refresh_current_playlist(cpl, cpv, mpdc);
                                // TODO clear search_view
                            }
                            break;
                        case user_event::TIMER_EXPIRED:
                            dimmed = true;
                            system(cfg.dim_idle_timer.dim_command.c_str());
                            continue;
                        default:
                            break;
                    }
                    // handle change events, but nothing else
                    // TODO not optimal since playlist changes are technically not necessary
                    if (dimmed)
                        continue;
                }
                else
                {
                    if (idle_timer_enabled(cfg))
                    {
                        if (dimmed)
                        {
                            if (current_playlist_needs_refresh)
                            {
                                refresh_current_playlist(cpl, cpv, mpdc);
                                // TODO clear search_view
                                current_playlist_needs_refresh = false;
                            }
                            // ignore one event, turn on lights
                            dimmed = false;
                            system(cfg.dim_idle_timer.undim_command.c_str());
                            iti.sync();
                            // TODO refactor into class
                            SDL_AddTimer(std::chrono::milliseconds(cfg.dim_idle_timer.delay).count(), idle_timer_cb, &iti);
                            // force a refresh by rebranding it - kind of hacky
                            ev.type = user_event_type;
                            ev.user.code = static_cast<int>(user_event::REFRESH);
                        }
                        else
                        {
                            iti.signal_user_activity();
                            ctx.process_event(ev);
                        }
                    }
                    else
                    {
                        ctx.process_event(ev);
                    }
                }


                if (refresh_cover)
                {
                    cover_type ct;
                    if (cfg.cover.directory.has_value())
                    {
                        auto opt_cover_path = find_cover_file(current_song_path, cfg.cover.directory.value(), cfg.cover.names, cfg.cover.extensions);

                        if (opt_cover_path.has_value())
                        {
                            ct = cover_type::IMAGE;
                            cv->set_cover(std::move(load_texture_from_image(renderer, opt_cover_path.value())));
                        }
                        else
                        {
                            ct = cover_type::SONG_INFO;
                        }
                    }
                    else
                    {
                        ct = cover_type::SONG_INFO;
                    }

                    if (ct == cover_type::SONG_INFO)
                    {
                        cv->set_cover(mpdc.get_current_title(), mpdc.get_current_artist(), mpdc.get_current_album());
                    }
                    refresh_cover = false;

                }

                ctx.draw();
            }
        }

        mpdc.stop();
        mpdc_thread.join();
    }

    TTF_Quit();

    SDL_DestroyWindow(window);
    SDL_Quit();

    return result;
}

int main(int argc, char * argv[])
{
    quit_action quit_action = quit_action::NONE;

    char const * shutdown_command;
    char const * reboot_command;

    {
        bool found = false;
        program_config cfg;
        for (auto const & cfg_dir : get_config_directories())
        {
            auto cfg_path = cfg_dir / "mpd-touch-screen-gui.conf";

            if (boost::filesystem::exists(cfg_path) && parse_program_config(cfg_path, cfg))
            {
                found = true;
                quit_action = program(cfg);
                shutdown_command = cfg.system_control.shutdown_command.c_str();
                reboot_command = cfg.system_control.reboot_command.c_str();
                break;
            }
        }

        if (!found)
        {
            std::cerr << "Failed to parse configuration file." << std::endl;
            return 1;
        }
    }

    if (quit_action == quit_action::SHUTDOWN)
        std::system(shutdown_command);
    else if (quit_action == quit_action::REBOOT)
        std::system(reboot_command);

    return 0;
}

