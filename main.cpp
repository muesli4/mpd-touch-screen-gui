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

#include "mpd_control.hpp"
#include "util.hpp"
#include "sdl_util.hpp"
#include "font_atlas.hpp"
#include "gui.hpp"

#ifndef __arm__
// test build to run on local machine
#define TEST_BUILD
#endif

// enable dimming functionality
#define DIM_IDLE_TIMER

// TODO when hardware rendering is available replace blits with texture copy of the renderer
//      (for software it doesn't make sense, since the renderer will just make a copy of the surface to generate a texture)
// TODO playlist: fade out song titles when over boundary or move source rectangle with time
// TODO playlist: use several tags and display in different cells:
//          - use a header colum (expand when clicked)
//          - scroll with swipe
// TODO move constants into config

#ifdef TEST_BUILD
char const * const BASE_DIR = "/home/moritz/Musik/";
char const * const SHUTDOWN_CMD = "echo Shutting down";
char const * const REBOOT_CMD = "echo Rebooting";
char const * const DIM_CMD = "echo dimming";
char const * const UNDIM_CMD = "echo undimming";
#else
char const * const BASE_DIR = "/mnt/music/library/";
char const * const SHUTDOWN_CMD = "sudo poweroff";
char const * const REBOOT_CMD = "sudo reboot";
char const * const DIM_CMD = "display-pwm.sh 1024";
char const * const UNDIM_CMD = "display-pwm.sh 0";
#endif

#ifdef DIM_IDLE_TIMER
// after the delay without activity DIM_CMD is executed, once user input happens
// again, UNDIM_CMD is executed
std::chrono::milliseconds const IDLE_TIMER_DELAY_MS(60000);
#endif

// determines the minimum length of a swipe
unsigned int const SWIPE_THRESHOLD_LOW_X = 30;
unsigned int const SWIPE_THRESHOLD_LOW_Y = SWIPE_THRESHOLD_LOW_X;

// determines how unambiguous a swipe has to be to count as either horizontal or vertical
double const DIR_UNAMBIG_FACTOR_THRESHOLD = 0.3;

// the time to wait after a swipe, before allowing touch events
unsigned int const SWIPE_WAIT_DEBOUNCE_MS_THRESHOLD = 400;

// determines how long a swipe is still recognized as a touch
unsigned int const TOUCH_DISTANCE_THRESHOLD_HIGH = 10;

// cover extensions and names in order of preference
std::array<char const * const, 3> const cover_extensions = { "png", "jpeg", "jpg" };
std::array<char const * const, 3> const cover_names = { "front", "cover", "back" };

SDL_Surface * create_cover_replacement(SDL_Surface const * s, SDL_Rect brect, font_atlas & fa, std::string title, std::string artist, std::string album)
{
    SDL_Surface * target_surface = create_similar_surface(s, brect.w, brect.h);

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

SDL_Surface * load_cover(std::string rel_song_dir_path)
{
    std::string const abs_cover_dir = absolute_cover_path(BASE_DIR, basename(rel_song_dir_path));
    SDL_Surface * cover;
    for (auto const & name : cover_names)
    {
        for (auto const & ext : cover_extensions)
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

std::pair<cover_type, unique_surface_ptr> create_cover(int w, int h, std::string song_path, SDL_Surface const * s, font_atlas & fa, mpd_control & mpdc)
{
    SDL_Rect cover_rect { 0, 0, w, h};
    SDL_Surface * cover_surface;
    SDL_Surface * img_surface = load_cover(song_path);
    if (img_surface != nullptr)
    {
        cover_surface = create_similar_surface(s, cover_rect.w, cover_rect.h);
        blit_preserve_ar(img_surface, cover_surface, &cover_rect);
        return std::make_pair(cover_type::IMAGE, unique_surface_ptr(cover_surface));
    }
    else
    {
        return std::make_pair(
            cover_type::SONG_INFO, 
            unique_surface_ptr(create_cover_replacement(s, cover_rect, fa, mpdc.get_current_title(), mpdc.get_current_artist(), mpdc.get_current_album()))
        );
    }
}


struct song
{
    song(std::string t, std::string ar, std::string al, std::string p)
        : title(t)
        , artist(ar)
        , album(al)
        , path(p)
    {
    }
    std::string title;
    std::string artist;
    std::string album;
    std::string path;
};

void handle_action(swipe_action a, mpd_control & mpdc, unsigned int volume_step)
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
    PLAYLIST,
    SONG_SEARCH,
    SHUTDOWN
};

view_type cycle_view_type(view_type v)
{
    return static_cast<view_type>((static_cast<unsigned int>(v) + 1) % 4);
}

enum class user_event
{
    RANDOM_CHANGED,
    SONG_CHANGED,
    PLAYLIST_CHANGED,
#ifdef DIM_IDLE_TIMER
    TIMER_EXPIRED,
#endif
    REFRESH
};

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

#ifdef DIM_IDLE_TIMER
//
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
#endif

bool is_quit_event(SDL_Event const & ev)
{
    return ev.type == SDL_QUIT
#ifdef TEST_BUILD
           || (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_q)
#endif
           ;

}

void refresh_current_playlist(std::vector<std::string> & cpl, unsigned int & cpv, mpd_control & mpdc)
{
    playlist_change_info pci = mpdc.get_current_playlist_changes(cpv);
    cpl.resize(pci.new_length);
    cpv = pci.new_version;
    for (auto & p : pci.changed_positions)
    {
        cpl[p.first].swap(p.second);
    }
}

int main(int argc, char * argv[])
{
    // TODO move to config
    char const * const DEFAULT_FONT_PATH = "/usr/share/fonts/TTF/DejaVuSans.ttf";

    quit_action quit_action = quit_action::NONE;

    // allows swipes with multiple lines, as long as the time between them is below this TODO not implemented
    //unsigned int const TOUCH_DEBOUNCE_TIME_MS_THRESHOLD_MAX = 200;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS
#ifdef DIM_IDLE_TIMER
                            | SDL_INIT_TIMER
#endif
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

    // font rendering setup
    if (TTF_Init() == -1)
    {
        std::cerr << "Could not initialize font rendering:"
                  << TTF_GetError() << '.' << std::endl;
        std::exit(0);
    }
    else
    {
        std::atexit(TTF_Quit);
    }

    TTF_Font * font = TTF_OpenFont(DEFAULT_FONT_PATH, 20);

    if (font == 0)
    {
        std::cerr << "Could not load font:"
                  << TTF_GetError() << '.' << std::endl;
        std::exit(0);
    }

    // create window
    SDL_Window * window = SDL_CreateWindow
        ( "mpc-touch-lcd-gui"
        , SDL_WINDOWPOS_UNDEFINED
        , SDL_WINDOWPOS_UNDEFINED
#ifdef TEST_BUILD
        , 320
        , 240
        , 0
#else
        , mode.w
        , mode.h
        , SDL_WINDOW_FULLSCREEN
#endif
        );

    SDL_Surface * screen = SDL_GetWindowSurface(window);


    // loop state
    std::string current_song_path;
    bool random;
    view_type current_view = view_type::COVER_SWIPE;
    bool view_dirty = false;
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

    unique_surface_ptr cover_surface_ptr;
    cover_type cover_type;

    // TODO check for error?
    uint32_t const user_event_type = SDL_RegisterEvents(1);
#ifdef DIM_IDLE_TIMER
    idle_timer_info iti(user_event_type);
    bool dimmed = true;
    // act as if the timer expired and it should dim now
    push_user_event(user_event_type, user_event::TIMER_EXPIRED);
#endif

    mpd_control mpdc(
        [&](std::string const & uri, unsigned int pos)
        {
            push_user_event(user_event_type, user_event::SONG_CHANGED);
            current_song_path = uri;
            current_song_pos = pos;
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

    {
        std::pair<std::vector<std::string>, unsigned int> playlist_data = mpdc.get_current_playlist();
        cpl.swap(playlist_data.first);
        cpv = playlist_data.second;
    }

    {
        font_atlas fa(DEFAULT_FONT_PATH, 20);
        font_atlas fa_small(DEFAULT_FONT_PATH, 15);
        gui_event_info gei;
        gui_context gc(gei, screen, DIR_UNAMBIG_FACTOR_THRESHOLD, TOUCH_DISTANCE_THRESHOLD_HIGH, SWIPE_WAIT_DEBOUNCE_MS_THRESHOLD);

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
            else if (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP
                                                    || ev.type == user_event_type
#ifdef TEST_BUILD
                                                    || ev.type == SDL_WINDOWEVENT
#endif
                    )
            {

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
                                refresh_current_playlist(cpl, cpv, mpdc);
                            break;
#ifdef DIM_IDLE_TIMER
                        case user_event::TIMER_EXPIRED:
                            dimmed = true;
                            system(DIM_CMD);
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
                            current_playlist_needs_refresh = false;
                        }
                        // ignore one event, turn on lights
                        dimmed = false;
                        system(UNDIM_CMD);
                        iti.sync();
                        SDL_AddTimer(IDLE_TIMER_DELAY_MS.count(), idle_timer_cb, &iti);
                        continue;
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
                    SDL_Rect buttons_rect {0, 0, 40, screen->h};
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

                    SDL_UpdateWindowSurfaceRects(window, &buttons_rect, 1);
                }

                SDL_Rect const view_rect = {40, 0, screen->w - 40, screen->h};

                // view area
                if (current_view == view_type::COVER_SWIPE)
                {
                    swipe_action a = swipe_area(view_rect, gc);

                    // redraw cover if it is a new one or if marked dirty
                    if (view_dirty || !cover_surface_ptr)
                    {
                        if (!cover_surface_ptr)
                            std::tie(cover_type, cover_surface_ptr) = create_cover(view_rect.w, view_rect.h, current_song_path, screen, fa, mpdc);
                        SDL_Rect r = view_rect;
                        SDL_BlitSurface(cover_surface_ptr.get(), nullptr, screen, &r);
                        SDL_UpdateWindowSurfaceRects(window, &view_rect, 1);
                        view_dirty = false;
                    }
                    handle_action(a, mpdc, 5);
                }
                else
                {
                    gc.draw_background(view_rect);

                    if (current_view == view_type::SHUTDOWN)
                    {
                        quit_action = shutdown_view(view_rect, fa, gc);
                        if (quit_action != quit_action::NONE)
                            run = false;
                    }
                    else
                    {
                        v_layout vl(6, 4, pad_box(view_rect, 4));
                        auto top_box = vl.box(5);
                        vl.next(5);

                        unsigned int const item_skip = list_view_visible_items(top_box, fa_small);

                        if (current_view == view_type::PLAYLIST)
                        {

                            h_layout hl(3, 4, vl.box());

                            if (text_button(hl.box(), "Jump", fa, gc))
                                cpl_view_pos = current_song_pos >= 5 ? std::min(current_song_pos - 5, static_cast<unsigned int>(cpl.size() - item_skip)) : 0;
                            hl.next();
                            if (text_button(hl.box(), "▲", fa, gc))
                                cpl_view_pos = dec_ensure_lower(cpl_view_pos - item_skip, cpl_view_pos, 0);
                            hl.next();
                            if (text_button(hl.box(), "▼", fa, gc))
                                cpl_view_pos = inc_ensure_upper(cpl_view_pos + item_skip, cpl_view_pos, cpl.size() < item_skip ? 0 : cpl.size() - item_skip);

                            int selection = list_view(top_box, cpl, cpl_view_pos, current_song_pos, fa_small, gc);
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
                                    // TODO Down works with small result
                                    search_items_view_pos = inc_ensure_upper(search_items_view_pos + item_skip, search_items_view_pos, search_items.size() < item_skip ? 0 : search_items.size() - item_skip);

                                int selection = list_view(top_box, search_items, search_items_view_pos, -1, fa_small, gc);

                                if (selection != -1)
                                    mpdc.play_position(search_item_positions[selection]);
                            }
                            else
                            {
                                v_layout vl(6, 4, pad_box(view_rect, 4));

                                auto top_box = vl.box();

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
                                        // get a single utf8 encoded unicode character
                                        char buff[8];

                                        int const num_bytes = next_utf8(buff, row_ptr);

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
                    SDL_UpdateWindowSurfaceRects(window, &view_rect, 1);
                }
            }
        }
    }

    mpdc.stop();
    mpdc_thread.join();

    TTF_CloseFont(font);
    TTF_Quit();

    SDL_DestroyWindow(window);
    SDL_Quit();

    if (quit_action == quit_action::SHUTDOWN)
        std::system(SHUTDOWN_CMD);
    else if (quit_action == quit_action::REBOOT)
        std::system(REBOOT_CMD);

    return 0;
}

