#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <iterator>
#include <mutex>
#include <queue>
#include <thread>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <libconfig.h++>
#include <libwtk-sdl2/box.hpp>
#include <libwtk-sdl2/button.hpp>
#include <libwtk-sdl2/list_view.hpp>
#include <libwtk-sdl2/notebook.hpp>
#include <libwtk-sdl2/padding.hpp>
#include <libwtk-sdl2/sdl_util.hpp>
#include <libwtk-sdl2/util.hpp>
#include <libwtk-sdl2/widget.hpp>
#include <libwtk-sdl2/widget_context.hpp>

#include "config_file.hpp"
#include "idle_timer.hpp"
#include "mpd_control.hpp"
#include "navigation_event.hpp"
#include "program_config.hpp"
#include "udp_control.hpp"
#include "user_event.hpp"
#include "util.hpp"

#include "cover_view.hpp"
#include "search_view.hpp"
#include "list_view_controls.hpp"

// future feature list and ideas:
// TODO replace cycling with a menu
// TODO playlist view: use several tags and display in different cells:
//          - use a header colum (expand when clicked)
//          - scroll horizontally with swipe
// TODO playlist view: jump to song position on new song
// TODO cover view: show a popup when an action has been executed (use timer to refresh)
//                  this may also be used for a remote control, if a popup exists draw it
//                  over everything, remove the popup with a timer by simply sending a refresh event
//                  (may use a specialized popup widget for that!)

// determines how long a swipe is still recognized as a touch
unsigned int const TOUCH_DISTANCE_THRESHOLD_HIGH = 10;

std::optional<std::string> find_cover_file(std::string rel_song_dir_path, std::string base_dir, std::vector<std::string> names, std::vector<std::string> extensions)
{
    std::string const abs_cover_dir = absolute_cover_path(base_dir, basename(rel_song_dir_path));
    for (auto const & name : names)
    {
        for (auto const & ext : extensions)
        {
            std::string const cover_path = abs_cover_dir + '/' + name + "." + ext;
            if (boost::filesystem::exists(boost::filesystem::path(cover_path)))
            {
                return cover_path;
            }
        }
    }

    return std::nullopt;
}

void handle_cover_swipe_direction(swipe_direction dir, mpd_control & mpdc, unsigned int volume_step)
{
    switch (dir)
    {
        case swipe_direction::UP:
            mpdc.inc_volume(volume_step);
            break;
        case swipe_direction::DOWN:
            mpdc.dec_volume(volume_step);
            break;
        case swipe_direction::RIGHT:
            mpdc.next_song();
            break;
        case swipe_direction::LEFT:
            mpdc.prev_song();
            break;
        default:
            break;
    }
}

enum quit_action
{
    SHUTDOWN,
    REBOOT,
    NONE
};

template <typename T, typename EventSender, typename Event>
void push_change_event(EventSender & es, Event e, T & old_val, T const & new_val)
{
    if (old_val != new_val)
    {
        old_val = new_val;
        es.push(e);
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
    return vbox({ { false, std::make_shared<button>("Shutdown", [&run, &result](){ result = quit_action::SHUTDOWN; run = false; }) }
                , { false, std::make_shared<button>("Reboot", [&run, &result](){ result = quit_action::REBOOT; run = false; }) }
                }, 5, true);
}

std::string random_label(bool random)
{
    return random ? "¬R" : "R";
}

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

enum class change_event_type
{
    RANDOM_CHANGED,
    SONG_CHANGED,
    PLAYLIST_CHANGED
};

void handle_other_event(SDL_Event const & e, widget_context & ctx, navigation_event_sender const & nes)
{
    if (nes.is_event_type(e.user.type))
    {
        navigation_event ne;
        nes.read(e, ne);
        if (ne.type == navigation_event_type::NAVIGATION)
        {
            ctx.navigate_selection(ne.nt);
        }
        else
        {
            ctx.activate();
        }
    }
    else
    {
        ctx.process_event(e);
    }
}

quit_action event_loop(SDL_Renderer * renderer, program_config const & cfg)
{
    quit_action result = quit_action::NONE;

    // loop state
    std::string current_song_path;

    bool random;

    unsigned int current_song_pos = 0;
    std::vector<std::string> cpl;
    unsigned int cpv;

    bool refresh_cover = true;
    bool current_playlist_needs_refresh = false;

    // Set up user events.
    enum_user_event_sender<change_event_type> ces;
    enum_user_event_sender<idle_timer_event_type> tes;
    navigation_event_sender nes;

    idle_timer_info iti(tes);
    bool dimmed = false;

    // timer is enabled
    if (idle_timer_enabled(cfg))
    {
        // act as if the timer expired and it should dim now
        tes.push(idle_timer_event_type::IDLE_TIMER_EXPIRED);
    }

    mpd_control mpdc(
        [&](std::optional<song_location> opt_sl)
        {
            // TODO probably move, as string copy might not be atomic
            //      another idea is to completely move all song information
            //      into the main thread
            if (opt_sl.has_value())
            {
                auto const & sl = opt_sl.value();
                current_song_path = sl.path;
                current_song_pos = sl.pos;
            }

            ces.push(change_event_type::SONG_CHANGED);
        },
        [&](bool value)
        {

            push_change_event(ces, change_event_type::RANDOM_CHANGED, random, value);
        },
        [&]()
        {
            ces.push(change_event_type::PLAYLIST_CHANGED);
        }
    );
    std::thread mpdc_thread(&mpd_control::run, std::ref(mpdc));

    std::optional<udp_control> opt_udp_control;
    std::thread udp_thread;
    if (cfg.opt_port.has_value())
    {
        opt_udp_control.emplace(cfg.opt_port.value(), nes);
        std::thread t { &udp_control::run, std::ref(opt_udp_control.value()) };
        udp_thread.swap(t);
    }

    // get initial state from mpd
    std::tie(cpl, cpv) = mpdc.get_current_playlist();
    random = mpdc.get_random();

    bool run = true;
    SDL_Event ev;

    // TODO move to MVC
    auto random_button = std::make_shared<button>(random_label(random), [&](){ mpdc.set_random(!random); });

    auto cv = std::make_shared<cover_view>([&](swipe_direction dir){ handle_cover_swipe_direction(dir, mpdc, 5); }, [&](){ mpdc.toggle_pause(); });
    auto playlist_v = std::make_shared<list_view>(cpl, current_song_pos, [&mpdc](std::size_t pos){ mpdc.play_position(pos); });
    auto search_v = std::make_shared<search_view>(cfg.on_screen_keyboard.size, cfg.on_screen_keyboard.keys, cpl, [&](auto pos){ mpdc.play_position(pos); });

    auto view_box = std::make_shared<notebook>(
        std::vector<widget_ptr>{ cv
                               , add_list_view_controls(playlist_v, "Jump", [=, &current_song_pos](){ playlist_v->set_position(current_song_pos); })
                               , search_v
                               , make_shutdown_view(result, run)
                               });

    // TODO introduce image button and add symbols from, e.g.: https://material.io/icons/
    auto button_controls = vbox(
            { { false, std::make_shared<button>("♫", [&](){ view_box->set_page((view_box->get_page() + 1) % 4);  }) }
            , { false, std::make_shared<button>("►", [&](){ mpdc.toggle_pause(); }) } // choose one of "❚❚"  "▍▍""▋▋"
            , { false, random_button }
            }, 5, true);

    box main_widget
        ( box::orientation::HORIZONTAL
        , { { false, pad_right(-5, pad(5, button_controls)) }
          , { true, pad(5, view_box) }
          }
        , 0
        );

    // TODO add dir_unambig_factor_threshold from config
    widget_context ctx(renderer, { cfg.default_font, cfg.big_font }, main_widget);
    ctx.draw();

    while (run && SDL_WaitEvent(&ev) == 1)
    {
        if (is_quit_event(ev))
        {
            std::cout << "Requested quit" << std::endl;
            run = false;
        }
        // most of the events are not required for a standalone fullscreen application
        else if (is_input_event(ev) || nes.is_event_type(ev.type)
                                    || ces.is_event_type(ev.type)
                                    || tes.is_event_type(ev.type)
                                    || ev.type == SDL_WINDOWEVENT
                )
        {
            // handle asynchronous user events synchronously
            if (ces.is_event_type(ev.type))
            {
                switch (ces.read(ev))
                {
                    case change_event_type::SONG_CHANGED:
                        refresh_cover = true;
                        playlist_v->set_highlight_position(current_song_pos);
                        search_v->set_filtered_highlight_position(current_song_pos);
                        break;
                    case change_event_type::PLAYLIST_CHANGED:
                        if (dimmed)
                            current_playlist_needs_refresh = true;
                        else
                        {
                            refresh_current_playlist(cpl, cpv, mpdc);

                            playlist_v->set_position(0);
                            search_v->on_playlist_changed();
                        }
                        break;
                    case change_event_type::RANDOM_CHANGED:
                        random_button->set_label(random_label(random));
                        break;
                    default:
                        break;
                }
                continue;
            }
            // dim idle timer expired
            else if (tes.is_event_type(ev.type))
            {
                dimmed = true;
                system(cfg.dim_idle_timer.dim_command.c_str());
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
                            current_playlist_needs_refresh = false;

                            playlist_v->set_position(0);
                            search_v->on_playlist_changed();
                        }
                        // ignore one event, turn on lights
                        dimmed = false;
                        system(cfg.dim_idle_timer.undim_command.c_str());
                        iti.sync();
                        // TODO refactor into class
                        SDL_AddTimer(std::chrono::milliseconds(cfg.dim_idle_timer.delay).count(), idle_timer_cb, &iti);
                    }
                    else
                    {
                        iti.signal_user_activity();
                        handle_other_event(ev, ctx, nes);
                    }
                }
                else
                {
                    handle_other_event(ev, ctx, nes);
                }
            }


            if (refresh_cover)
            {
                cover_type ct;
                if (cfg.cover.opt_directory.has_value())
                {
                    auto opt_cover_path = find_cover_file(current_song_path, cfg.cover.opt_directory.value(), cfg.cover.names, cfg.cover.extensions);

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

    if (cfg.opt_port.has_value())
    {
        opt_udp_control.value().stop();
        udp_thread.join();
    }

    return result;
}

SDL_Window * create_window_with_config(display_config const & cfg)
{
    // Determine screen size of first display.
    SDL_DisplayMode mode = { SDL_PIXELFORMAT_UNKNOWN, 0, 0, 0, 0 };
    if (SDL_GetDisplayMode(0, 0, &mode) != 0)
        return nullptr;

    // Pick maximum resolution in fullscreen.
    if (!cfg.fullscreen)
    {
        mode.w = std::min(cfg.resolution.w, mode.w);
        mode.h = std::min(cfg.resolution.h, mode.h);
    }

    return SDL_CreateWindow
        ( "mpc-touch-screen-gui"
        , SDL_WINDOWPOS_UNDEFINED
        , SDL_WINDOWPOS_UNDEFINED
        , mode.w
        , mode.h
        , cfg.fullscreen ? SDL_WINDOW_FULLSCREEN : 0
        );
}

quit_action program(program_config const & cfg)
{
    // Initialize important libraries and then start the SDL2 event loop.

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS
                            | (idle_timer_enabled(cfg) ? SDL_INIT_TIMER : 0)
            );
    std::atexit(SDL_Quit);

    SDL_Window * window = create_window_with_config(cfg.display);
    if (window == nullptr)
    {
        std::cerr << "Could not create window: " << SDL_GetError() << std::endl;
        std::exit(0);
    }

    // font rendering setup
    if (TTF_Init() == -1)
    {
        std::cerr << "Could not initialize font rendering:"
                  << TTF_GetError() << '.' << std::endl;
        std::exit(0);
    }

    SDL_Renderer * renderer = renderer_from_window(window);

    quit_action result = quit_action::NONE;

    try
    {
         result = event_loop(renderer, cfg);
    }
    catch (std::exception const & e)
    {
        std::cerr << e.what() << std::endl;
    }

    TTF_Quit();

    SDL_DestroyWindow(window);
    SDL_Quit();

    return result;
}

int main(int argc, char * argv[])
{
    std::cout << PACKAGE_NAME << ' ' << VERSION << std::endl;

    quit_action quit_action = quit_action::NONE;

    char const * shutdown_command = "echo";
    char const * reboot_command = "echo";

    // Search for a configuration file and launch the program.
    {
        auto const & cfg_dirs = get_config_directories();

        if (cfg_dirs.empty())
        {
            std::cerr << "Unable to find configuration directory." << std::endl;
            return 1;
        }
        else
        {
            using namespace std;

            auto opt_cfg_path = find_or_create_config_file("program.conf");

            if (opt_cfg_path.has_value())
            {
                program_config cfg;
                try
                {
                    if (parse_program_config(opt_cfg_path.value(), cfg))
                    {
                        quit_action = program(cfg);
                        shutdown_command = cfg.system_control.shutdown_command.c_str();
                        reboot_command = cfg.system_control.reboot_command.c_str();
                    }
                    else
                    {
                        cerr << "Failed to parse configuration file." << endl;
                        return 1;
                    }
                }
                catch (libconfig::ConfigException const & e)
                {
                    cerr << e.what() << endl;
                }
            }

        }
    }

    if (quit_action == quit_action::SHUTDOWN)
        std::system(shutdown_command);
    else if (quit_action == quit_action::REBOOT)
        std::system(reboot_command);

    return 0;
}

