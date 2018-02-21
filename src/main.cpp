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
#include <libwtk-sdl2/box.hpp>
#include <libwtk-sdl2/button.hpp>
#include <libwtk-sdl2/list_view.hpp>
#include <libwtk-sdl2/notebook.hpp>
#include <libwtk-sdl2/padding.hpp>
#include <libwtk-sdl2/sdl_util.hpp>
#include <libwtk-sdl2/util.hpp>
#include <libwtk-sdl2/widget.hpp>
#include <libwtk-sdl2/widget_context.hpp>

#include "idle_timer.hpp"
#include "mpd_control.hpp"
#include "program_config.hpp"
#include "udp_control.hpp"
#include "user_event.hpp"
#include "util.hpp"

#include "cover_view.hpp"
#include "search_view.hpp"
#include "song_list_view.hpp"

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
            std::string const cover_path = abs_cover_dir + name + "." + ext;
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

template <typename T> void push_change_event(user_event_sender & ues, user_event ue, T & old_val, T const & new_val)
{
    if (old_val != new_val)
    {
        old_val = new_val;
        ues.push(ue);
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
    user_event_sender ues;

    idle_timer_info iti(ues);
    bool dimmed = false;

    // timer is enabled
    if (idle_timer_enabled(cfg))
    {
        // act as if the timer expired and it should dim now
        ues.push(user_event::TIMER_EXPIRED);
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
            ues.push(user_event::SONG_CHANGED);
        },
        [&](bool value)
        {

            push_change_event(ues, user_event::RANDOM_CHANGED, random, value);
        },
        [&]()
        {
            ues.push(user_event::PLAYLIST_CHANGED);
        }
    );

    udp_control udp_control(6666, ues);

    std::thread udp_thread(&udp_control::run, std::ref(udp_control));
    std::thread mpdc_thread(&mpd_control::run, std::ref(mpdc));

    // get initial state from mpd
    std::tie(cpl, cpv) = mpdc.get_current_playlist();
    random = mpdc.get_random();

    bool run = true;
    SDL_Event ev;

    // TODO move to MVC
    auto random_button = std::make_shared<button>(random_label(random), [&](){ mpdc.set_random(!random); });

    auto cv = std::make_shared<cover_view>([&](swipe_direction dir){ handle_cover_swipe_direction(dir, mpdc, 5); }, [&](){ mpdc.toggle_pause(); });
    auto slv = std::make_shared<list_view>(cpl, current_song_pos, [&mpdc](std::size_t pos){ mpdc.play_position(pos); });
    auto sv = std::make_shared<search_view>(cfg.on_screen_keyboard.size, cfg.on_screen_keyboard.keys, cpl, [&](auto pos){ mpdc.play_position(pos); });

    auto view_box = std::make_shared<notebook>(
        std::vector<widget_ptr>{ cv
                               , song_list_view(slv, "Jump", [=, &current_song_pos](){ slv->set_position(current_song_pos); })
                               , sv
                               , make_shutdown_view(result, run)
                               });

    // TODO introduce image button and add symbols from, e.g.: https://material.io/icons/
    auto button_controls = vbox(
            { { false, std::make_shared<button>("♫", [&](){ view_box->set_page((view_box->get_page() + 1) % 4);  }) }
            , { false, std::make_shared<button>("►", [&](){ mpdc.toggle_pause(); }) } // choose one of "❚❚"  "▍▍""▋▋"
            , { false, random_button }
            }, 5);

    box main_widget
        ( box::orientation::HORIZONTAL
        , { { false, pad(5, button_controls) }
          , { true, pad(5, view_box) }
          }
        , 0
        , false
        );

    // TODO add dir_unambig_factor_threshold from config
    widget_context ctx(renderer, cfg.default_font, main_widget);
    ctx.draw();


    while (run && SDL_WaitEvent(&ev) == 1)
    {
        if (is_quit_event(ev))
        {
            std::cout << "Requested quit" << std::endl;
            run = false;
        }
        // most of the events are not required for a standalone fullscreen application
        else if (is_input_event(ev) || ues.is(ev.type)
                                    || ev.type == SDL_WINDOWEVENT
                )
        {
            // handle asynchronous user events synchronously
            if (ues.is(ev.type))
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

                            slv->set_position(0);
                            sv->on_playlist_changed();
                        }
                        break;
                    case user_event::RANDOM_CHANGED:
                        random_button->set_label(random_label(random));
                        break;
                    case user_event::TIMER_EXPIRED:
                        dimmed = true;
                        system(cfg.dim_idle_timer.dim_command.c_str());
                        continue;
                    case user_event::ACTIVATE:
                        ctx.activate();
                        break;
                    case user_event::NAVIGATION:
                        {
                            std::uintptr_t const p = reinterpret_cast<std::uintptr_t>(ev.user.data1);
                            navigation_type const nt = static_cast<navigation_type>(p);
                            ctx.navigate_selection(nt);
                        }
                        break;
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
                            current_playlist_needs_refresh = false;

                            slv->set_position(0);
                            sv->on_playlist_changed();
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
    udp_control.stop();
    udp_thread.join();

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
            using namespace boost::filesystem;

            auto const cfg_name = "program.conf";
            auto const cfg_rel_path = PACKAGE_NAME;

            optional<path> opt_cfg_path;

            for (auto const & cfg_dir : cfg_dirs)
            {
                auto tmp_path = cfg_dir / cfg_rel_path / cfg_name;

                if (exists(tmp_path))
                {
                    cout << "Found configuration file: " << tmp_path.c_str() << endl;
                    opt_cfg_path = tmp_path;
                    break;
                }
            }

            path cfg_path;
            if (opt_cfg_path.has_value())
            {
                cfg_path = opt_cfg_path.value();
            }
            else
            {
                boost::system::error_code ec;

                // Copy the default configuration to the best path.
                auto cfg_base_path = cfg_dirs.front() / cfg_rel_path;

                if (create_directories(cfg_base_path, ec))
                {
                    cout << "Created directory: " << cfg_base_path.c_str() << endl;
                }

                if (ec != boost::system::errc::success)
                {
                    cerr << "Failed to create config directory: " << ec.message() << endl;
                    return 1;
                }

                cfg_path = cfg_base_path / cfg_name;
                copy(path(PKGDATA) / cfg_name, cfg_path, ec);
                if (ec != boost::system::errc::success)
                {
                    cerr << "Failed to create configuration file: " << ec.message() << endl;
                    return 1;
                }
                else
                {

                    cout << "Created configuration file: " << cfg_path.c_str() << endl;
                }
            }

            program_config cfg;
            if (parse_program_config(cfg_path, cfg))
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
    }

    if (quit_action == quit_action::SHUTDOWN)
        std::system(shutdown_command);
    else if (quit_action == quit_action::REBOOT)
        std::system(reboot_command);

    return 0;
}

