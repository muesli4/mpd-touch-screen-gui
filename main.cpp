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

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include <unicode/unistr.h>

#include <experimental/filesystem>

#include "mpd_control.hpp"
#include "util.hpp"
#include "sdl_util.hpp"
#include "font_atlas.hpp"
#include "gui.hpp"
#include "program_config.hpp"

// future feature list and ideas:
// TODO replace cycling with a menu
// TODO when hardware rendering is available replace blits with texture copy of the renderer
//      for software it doesn't make sense, since the renderer will just make a
//      copy of the surface to generate a pseudo-texture
// TODO list view: fade out overlong text when over boundary or move source
//                 rectangle with time / alternatively use manual controls (e.g., horizontal swipe)
// TODO playlist view: use several tags and display in different cells:
//          - use a header colum (expand when clicked)
//          - scroll with swipe
// TODO playlist view: jump to song position on new song
// TODO move constants into config
// TODO cover view: show a popup when an action has been executed (use timer to refresh)
//                  this may also be used for a remote control, if a popup exists draw it
//                  over everything, remove the popup with a timer by simply sending a refresh event

// enable dimming functionality
//#define DIM_IDLE_TIMER

// determines the minimum length of a swipe
unsigned int const SWIPE_THRESHOLD_LOW_X = 30;
unsigned int const SWIPE_THRESHOLD_LOW_Y = SWIPE_THRESHOLD_LOW_X;

// the time to wait after a swipe, before allowing touch events
std::chrono::milliseconds const SWIPE_WAIT_DEBOUNCE_THRESHOLD(400);

// determines how long a swipe is still recognized as a touch
unsigned int const TOUCH_DISTANCE_THRESHOLD_HIGH = 10;

SDL_Surface * create_cover_replacement(uint32_t pfe, SDL_Rect brect, font_atlas & fa, std::string title, std::string artist, std::string album)
{
    SDL_Surface * target_surface = create_surface(pfe, brect.w, brect.h);

    SDL_FillRect(target_surface, nullptr, SDL_MapRGB(target_surface->format, 0, 0, 0));

    std::array<std::string const, 3> lines = { title, artist, album };

    int const offset = fa.font_line_skip() / 2;

    int y_offset = offset;

    for (auto & line : lines)
    {
        if (!line.empty())
        {
            auto text_surface_ptr = fa.text(line, brect.w - offset);

            if (text_surface_ptr)
            {
                SDL_Rect r = { offset, y_offset, text_surface_ptr->w, text_surface_ptr->h };
                SDL_BlitSurface(text_surface_ptr.get(), 0, target_surface, &r);
                y_offset += text_surface_ptr->h + offset;
            }
        }
    }

    return target_surface;
}

SDL_Surface * load_cover(std::string rel_song_dir_path, std::string base_dir, std::vector<std::string> names, std::vector<std::string> extensions)
{
    std::string const abs_cover_dir = absolute_cover_path(base_dir, basename(rel_song_dir_path));
    SDL_Surface * cover;
    for (auto const & name : names)
    {
        for (auto const & ext : extensions)
        {
            std::string cover_path = abs_cover_dir + name + "." + ext;
            cover = IMG_Load(cover_path.c_str());
            if (cover) return cover;
        }
    }

    return nullptr;
}

enum class cover_type
{
    IMAGE,
    SONG_INFO
};

std::pair<cover_type, unique_surface_ptr> create_cover(int w, int h, std::string song_path, uint32_t pfe, font_atlas & fa, mpd_control & mpdc, cover_config const & cfg)
{
    SDL_Rect cover_rect { 0, 0, w, h};
    SDL_Surface * cover_surface;
    SDL_Surface * img_surface = nullptr;

    if (cfg.directory.has_value())
        img_surface = load_cover(song_path, cfg.directory.value(), cfg.names, cfg.extensions);

    if (img_surface != nullptr)
    {
        cover_surface = create_surface(pfe, cover_rect.w, cover_rect.h);
        blit_preserve_ar(img_surface, cover_surface, &cover_rect);
        SDL_FreeSurface(img_surface);
        return std::make_pair(cover_type::IMAGE, unique_surface_ptr(cover_surface));
    }
    else
    {
        return std::make_pair(
            cover_type::SONG_INFO, 
            unique_surface_ptr(create_cover_replacement(pfe, cover_rect, fa, mpdc.get_current_title(), mpdc.get_current_artist(), mpdc.get_current_album()))
        );
    }
}

//struct song
//{
//    song(std::string t, std::string ar, std::string al, std::string p)
//        : title(t)
//        , artist(ar)
//        , album(al)
//        , path(p)
//    {
//    }
//    std::string title;
//    std::string artist;
//    std::string album;
//    std::string path;
//};

void handle_cover_swipe_action(swipe_action a, mpd_control & mpdc, unsigned int volume_step)
{
    switch (a)
    {
        case swipe_action::SWIPE_UP:
            mpdc.inc_volume(volume_step);
            break;
        case swipe_action::SWIPE_DOWN:
            mpdc.dec_volume(volume_step);
            break;
        case swipe_action::SWIPE_RIGHT:
            mpdc.next_song();
            break;
        case swipe_action::SWIPE_LEFT:
            mpdc.prev_song();
            break;
        case swipe_action::PRESS:
            mpdc.toggle_pause();
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

quit_action shutdown_view(SDL_Rect box, font_atlas & fa, gui_context & gc)
{
    v_layout l(2, 4, pad_box(box, 44));

    if (text_button(l.box(), "Shutdown", fa, gc))
    {
        return quit_action::SHUTDOWN;
    }
    l.next();
    if (text_button(l.box(), "Reboot", fa, gc))
    {
        return quit_action::REBOOT;
    }
    return quit_action::NONE;
}

enum class user_event
{
    RANDOM_CHANGED,
    SONG_CHANGED,
    PLAYLIST_CHANGED,
    TIMER_EXPIRED,
    REFRESH
};

void push_user_event(uint32_t event_type, user_event ue)
{
    SDL_Event e;
    SDL_memset(&e, 0, sizeof(e));
    e.type = event_type;
    e.user.code = (int)ue;
    SDL_PushEvent(&e);
}

template <typename T> void push_change_event(uint32_t event_type, user_event ue, T & old_val, T const & new_val)
{
    if (old_val != new_val)
    {
        old_val = new_val;
        push_user_event(event_type, ue);
    }
}

struct idle_timer_info
{
    idle_timer_info(uint32_t uet) : user_event_type(uet) {}

    uint32_t const user_event_type;

    void sync()
    {
        _start_tp = std::chrono::steady_clock::now();
        _last_activity_tp = _start_tp;
    }

    void signal_user_activity()
    {
        _last_activity_tp = std::chrono::steady_clock::now();
    }

    std::chrono::milliseconds remaining_ms()
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(_last_activity_tp - _start_tp);
    }

    private:

    std::chrono::time_point<std::chrono::steady_clock> _last_activity_tp;
    std::chrono::time_point<std::chrono::steady_clock> _start_tp;
};

Uint32 idle_timer_cb(Uint32 interval, void * iti_ptr)
{

    idle_timer_info & iti = *reinterpret_cast<idle_timer_info *>(iti_ptr);

    auto rem_ms = iti.remaining_ms();

    if (rem_ms.count() > 0)
    {
        iti.sync();
        return rem_ms.count();
    }
    else
    {
        push_user_event(iti.user_event_type, user_event::TIMER_EXPIRED);
        return 0;
    }
}

bool is_quit_event(SDL_Event const & ev)
{
    return ev.type == SDL_QUIT
           || (ev.type == SDL_KEYDOWN && ev.key.keysym.mod & KMOD_CTRL && ev.key.keysym.sym == SDLK_q)
           ;

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

quit_action program(program_config const & cfg)
{
    quit_action result;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS
                            | (cfg.dim_idle_timer.delay.count() == 0 ? 0 : SDL_INIT_TIMER)
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
    mode.w = std::min(cfg.display.resolution.w, mode.w);
    mode.h = std::min(cfg.display.resolution.h, mode.h);

    // font rendering setup
    if (TTF_Init() == -1)
    {
        std::cerr << "Could not initialize font rendering:"
                  << TTF_GetError() << '.' << std::endl;
        std::exit(0);
    }

    // create window
    SDL_Window * window = SDL_CreateWindow
        ( "mpc-touch-lcd-gui"
        , SDL_WINDOWPOS_UNDEFINED
        , SDL_WINDOWPOS_UNDEFINED
        , mode.w
        , mode.h
        , cfg.display.fullscreen ? SDL_WINDOW_FULLSCREEN : 0
        );

    uint32_t const pixel_format_enum = SDL_GetWindowPixelFormat(window);

    {
        // loop state
        std::string current_song_path;
        bool current_song_exists;

        bool random;
        view_type current_view = view_type::COVER_SWIPE;
        bool view_dirty = false;

        unique_surface_ptr cover_surface_ptr;
        cover_type cover_type;

        unsigned int current_song_pos = 0;
        unsigned int cpl_view_pos = 0;
        std::vector<std::string> cpl;
        unsigned int cpv;
        bool current_playlist_needs_refresh = false;

        bool present_search_results = false;
        std::string search_term;
        std::vector<std::string> search_items;
        unsigned int search_items_view_pos = 0;
        std::vector<int> search_item_positions;

        // TODO check for error?
        uint32_t const user_event_type = SDL_RegisterEvents(1);
#ifdef DIM_IDLE_TIMER
        idle_timer_info iti(user_event_type);
        bool dimmed = true;
        // act as if the timer expired and it should dim now
        push_user_event(user_event_type, user_event::TIMER_EXPIRED);
#endif

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

        font_atlas fa(cfg.font_path, 20);
        font_atlas fa_small(cfg.font_path, 15);
        gui_event_info gei;
        gui_context gc(gei, window, cfg.swipe.dir_unambig_factor_threshold, TOUCH_DISTANCE_THRESHOLD_HIGH, SWIPE_WAIT_DEBOUNCE_THRESHOLD);

        bool run = true;
        SDL_Event ev;

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
                            // TODO do not reset
                            // if (cover_type == cover_type::IMAGE && same album)
                            cover_surface_ptr.reset();
                            break;
                        case user_event::PLAYLIST_CHANGED:
#ifdef DIM_IDLE_TIMER
                            if (dimmed)
                                current_playlist_needs_refresh = true;
                            else
#endif
                            {
                                refresh_current_playlist(cpl, cpv, mpdc);
                                present_search_results = false;
                            }
                            break;
#ifdef DIM_IDLE_TIMER
                        case user_event::TIMER_EXPIRED:
                            dimmed = true;
                            system(cfg.dim_idle_timer.dim_command);
                            continue;
#endif
                        default:
                            break;
                    }
#ifdef DIM_IDLE_TIMER
                    // handle change events, but nothing else
                    // TODO not optimal since playlist changes are technically not necessary
                    if (dimmed)
                        continue;
#endif
                }
#ifdef DIM_IDLE_TIMER
                else
                {
                    if (dimmed)
                    {
                        if (current_playlist_needs_refresh)
                        {
                            refresh_current_playlist(cpl, cpv, mpdc);
                            present_search_results = false;
                            current_playlist_needs_refresh = false;
                        }
                        // ignore one event, turn on lights
                        dimmed = false;
                        system(cfg.dim_idle_timer.undim_command);
                        iti.sync();
                        SDL_AddTimer(std::chrono::milliseconds(cfg.dim_idle_timer.delay).count(), idle_timer_cb, &iti);
                        // force a refresh
                        ev.type = user_event_type;
                        ev.user.code = static_cast<int>(user_event::REFRESH);
                    }
                    else
                    {
                        iti.signal_user_activity();
                    }
                }
#endif
                apply_sdl_event(ev, gei, SWIPE_THRESHOLD_LOW_X, SWIPE_THRESHOLD_LOW_Y);

                // global buttons
                {
                    SDL_Rect buttons_rect {0, 0, 40, mode.h};
                    gc.draw_background(buttons_rect);
                    v_layout l(6, 4, pad_box(buttons_rect, 4));

                    if (text_button(l.box(), "♫", fa, gc))
                    {
                        current_view = cycle_view_type(current_view);
                        view_dirty = true;
                    }
                    l.next();
                    if (text_button(l.box(), "►", fa, gc))
                        mpdc.toggle_pause();
                    l.next();
                    if (text_button(l.box(), random ? "¬R" : "R", fa, gc))
                        mpdc.set_random(!random);

                }

                SDL_Rect const view_rect = {40, 0, mode.w - 40, mode.h};

                // view area
                if (current_view == view_type::COVER_SWIPE)
                {
                    swipe_action a = swipe_area(view_rect, gc);

                    // redraw cover if it is a new one or if marked dirty
                    if (view_dirty || !cover_surface_ptr)
                    {
                        if (current_song_exists)
                        {
                            if (!cover_surface_ptr)
                            {
                                std::tie(cover_type, cover_surface_ptr) = create_cover(view_rect.w, view_rect.h, current_song_path, pixel_format_enum, fa, mpdc, cfg.cover);

                            }
                            SDL_Rect r = view_rect;
                            gc.blit(cover_surface_ptr.get(), nullptr, &r);
                            view_dirty = false;
                        }
                        else
                            gc.draw_background(view_rect);
                    }
                    handle_cover_swipe_action(a, mpdc, 5);
                }
                else
                {
                    gc.draw_background(view_rect);

                    if (current_view == view_type::SHUTDOWN)
                    {
                        result = shutdown_view(view_rect, fa, gc);
                        if (result != quit_action::NONE)
                            run = false;
                    }
                    else
                    {
                        v_layout vl(6, 4, pad_box(view_rect, 4));
                        auto top_box = vl.box(5);
                        vl.next(5);

                        unsigned int const item_skip = list_view_visible_items(top_box, fa_small);

                        if (current_view == view_type::QUEUE)
                        {

                            h_layout hl(3, 4, vl.box());

                            if (text_button(hl.box(), "Jump", fa, gc))
                                cpl_view_pos = current_song_pos >= 5 && current_song_exists ? std::min(current_song_pos - 5, static_cast<unsigned int>(cpl.size() - item_skip)) : 0;
                            hl.next();
                            if (text_button(hl.box(), "▲", fa, gc))
                                cpl_view_pos = dec_ensure_lower(cpl_view_pos - item_skip, cpl_view_pos, 0);
                            hl.next();
                            if (text_button(hl.box(), "▼", fa, gc))
                                cpl_view_pos = inc_ensure_upper(cpl_view_pos + item_skip, cpl_view_pos, cpl.size() < item_skip ? 0 : cpl.size() - item_skip);

                            int selection = list_view(top_box, cpl, cpl_view_pos, current_song_exists ? current_song_pos : -1, fa_small, gc);
                            if (selection != -1)
                                mpdc.play_position(selection);
                        }
                        else if (current_view == view_type::SONG_SEARCH)
                        {
                            if (present_search_results)
                            {
                                h_layout hl(3, 4, vl.box());
                                if (text_button(hl.box(), "⌨", fa, gc))
                                {
                                    search_items.clear();
                                    search_item_positions.clear();
                                    search_term.clear();
                                    present_search_results = false;

                                    // necessary evil
                                    push_user_event(user_event_type, user_event::REFRESH);
                                }
                                hl.next();
                                if (text_button(hl.box(), "▲", fa, gc))
                                    search_items_view_pos = dec_ensure_lower(search_items_view_pos - item_skip, search_items_view_pos, 0);
                                hl.next();
                                if (text_button(hl.box(), "▼", fa, gc))
                                    search_items_view_pos = inc_ensure_upper(search_items_view_pos + item_skip, search_items_view_pos, search_items.size() < item_skip ? 0 : search_items.size() - item_skip);

                                int selection = list_view(top_box, search_items, search_items_view_pos, -1, fa_small, gc);

                                if (selection != -1)
                                    mpdc.play_position(search_item_positions[selection]);
                            }
                            else
                            {
                                v_layout vl(6, 4, pad_box(view_rect, 4));

                                auto top_box = vl.box();

                                // TODO read from config
                                // draw keys
                                std::array<char const * const, 5> letters
                                    { "abcdef"
                                    , "ghijkl"
                                    , "mnopqr"
                                    , "stuvwx"
                                    , "yzäöü "
                                    };
                                vl.next();

                                for (char const * row_ptr : letters)
                                {
                                    h_layout hl(6, 4, vl.box());
                                    while (*row_ptr != 0)
                                    {
                                        char buff[8];

                                        int const num_bytes = fetch_utf8(buff, row_ptr);

                                        if (num_bytes == 0)
                                            break;
                                        else
                                        {
                                            if (text_button(hl.box(), buff, fa, gc))
                                                search_term += buff;
                                            hl.next();
                                            row_ptr += num_bytes;
                                        }
                                    }
                                    vl.next();
                                }

                                // render controls (such that a redraw is not necessary)
                                {
                                    h_layout hl(6, 4, top_box);
                                    auto search_term_box = hl.box(4);

                                    hl.next(4);

                                    if (text_button(hl.box(), "⌫", fa, gc) && !search_term.empty())
                                    {
                                        int const len = count_utf8_backwards(search_term.c_str() + search_term.size() - 1);
                                        search_term.resize(search_term.size() - len);
                                    }
                                    hl.next();
                                    if (text_button(hl.box(), "⌧", fa, gc))
                                        search_term.clear();

                                    if (text_button(search_term_box, '\'' + search_term + '\'', fa, gc))
                                    {
                                        // do search
                                        search_item_positions.clear();
                                        for (std::size_t pos = 0; pos < cpl.size(); pos++)
                                        {
                                            auto const & s = cpl[pos];
                                            if (icu::UnicodeString::fromUTF8(s).toLower().indexOf(icu::UnicodeString::fromUTF8(search_term)) != -1)
                                            {
                                                search_item_positions.push_back(pos);
                                                search_items.push_back(s);
                                            }
                                        }

                                        present_search_results = true;
                                        search_items_view_pos = 0;

                                        // necessary evil
                                        push_user_event(user_event_type, user_event::REFRESH);
                                    }

                                }
                            }
                        }
                    }
                }
                gc.present();
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

