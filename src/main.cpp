#ifndef PACKAGE_NAME
#define PACKAGE_NAME "mpd-touch-screen-gui"
#endif
#ifndef VERSION
#define VERSION "unknown version"
#endif

#include <iostream>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <libconfig.h++>
#include <libwtk-sdl2/sdl_util.hpp>

#include "config_file.hpp"
#include "event_loop.hpp"
#include "program_config.hpp"
#include "util.hpp"

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

SDL_Window * create_window_from_config(display_config const & cfg)
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

    SDL_Window * window = create_window_from_config(cfg.display);
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

