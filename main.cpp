// poor mans mpd cover display (used for ILI9341 320x240 pixel display)

#include <iostream>
#include <cstring>
#include <functional>
#include <queue>
#include <array>

#include <iterator>
#include <algorithm>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include "mpd_control.hpp"
#include "util.hpp"

#ifndef __arm__
char const base_dir [] = "/home/moritz/Musik/";
#else
char const base_dir [] = "/mnt/music/library/";
#endif

unsigned int const SWIPE_THRESHOLD_LOW_X = 30;
unsigned int const SWIPE_THRESHOLD_LOW_Y = SWIPE_THRESHOLD_LOW_X;

// determines how ambiguous a swipe has to be
double const DIR_UNAMBIG_FACTOR_THRESHOLD = 0.3;

// the time to wait after a swipe, before allowing touch events
unsigned int SWIPE_WAIT_DEBOUNCE_MS_THRESHOLD = 400;

// determines how long a swipe is still recognized as a touch
unsigned int const TOUCH_DISTANCE_THRESHOLD_HIGH = 10;

int const base_dir_len = sizeof(base_dir);

std::array<std::string const, 3> const exts = { "png", "jpeg", "jpg" };
std::array<std::string const, 3> const names = { "front", "cover", "back" };

void show_rect(SDL_Rect const & r)
{
    std::cout << r.x << " " << r.y << " on " << r.w << "x" << r.h << std::endl;
}

std::string cover_abs_path(std::string rel_song_dir_path)
{
    // try to detect, whether we need to look for the album cover in the super directory
    std::size_t const super_dir_sep_pos = rel_song_dir_path.find_last_of(PATH_SEP, rel_song_dir_path.size() - 2);

    bool has_discnumber = false;
    if (super_dir_sep_pos != std::string::npos)
    {
        std::string const super_dir =
            rel_song_dir_path.substr(super_dir_sep_pos + 1, rel_song_dir_path.size() - 2 - super_dir_sep_pos);

        if (super_dir.find("CD ") == 0
            && std::all_of(std::next(super_dir.begin(), 3), super_dir.end(), ::isdigit)
           )
        {
            has_discnumber = true;
        }
    }

    return std::string(base_dir) + (has_discnumber ? rel_song_dir_path.substr(0, super_dir_sep_pos + 1) : rel_song_dir_path);
}

// blit a surface to another surface while preserving aspect ratio
void blit_preserve_ar(SDL_Surface * source, SDL_Surface * dest, SDL_Rect * destrect)
{
    int const w = destrect->w;
    int const h = destrect->h;

    int const sw = source->w;
    int const sh = source->h;

    double const dest_ratio = static_cast<double>(w) / static_cast<double>(h);
    double const source_ratio = static_cast<double>(sw) / static_cast<double>(sh);

    bool const dest_wider = dest_ratio >= source_ratio;

    // remaining width and height
    int const rw = dest_wider ? source_ratio * h : w;
    int const rh = dest_wider ? h : (w / source_ratio);

    int const pad_x = (w - rw) / 2;
    int const pad_y = (h - rh) / 2;

    SDL_Rect cover_rect = { 0, 0, sw, sh };
    SDL_Rect dest_pos = { destrect->x + pad_x, destrect->y + pad_y, rw, rh };
    SDL_BlitScaled(source, &cover_rect, dest, &dest_pos);

    // fill remaining rects
    if (dest_wider)
    {
        SDL_Rect left = { destrect->x, destrect->y, pad_x, h };
        SDL_FillRect(dest, &left, SDL_MapRGB(dest->format, 0, 0, 0));

        SDL_Rect right = { destrect->x + w - pad_x, destrect->y, pad_x, h };
        SDL_FillRect(dest, &right, SDL_MapRGB(dest->format, 0, 0, 0));
    }
    else
    {
        SDL_Rect top = { destrect->x, destrect->y, w, pad_y };
        SDL_FillRect(dest, &top, SDL_MapRGB(dest->format, 0, 0, 0));

        SDL_Rect bottom = { destrect->x, destrect->y + h - pad_y, w, pad_y };
        SDL_FillRect(dest, &bottom, SDL_MapRGB(dest->format, 0, 0, 0));
    }
}

void draw_cover_replacement(SDL_Surface * surface, SDL_Rect brect, TTF_Font * font, std::string title, std::string artist, std::string album)
{
    SDL_FillRect(surface, 0, SDL_MapRGB(surface->format, 0, 0, 0));

    SDL_Color font_color = { 255, 255, 255, 255 };

    std::array<std::string const, 3> lines = { title, artist, album };

    int y_offset = 20;

    for (auto & line : lines)
    {
        SDL_Surface * text_surface =
            TTF_RenderUTF8_Blended( font
                                    , line.c_str()
                                    , font_color
                                    );

        if (text_surface != 0)
        {
            SDL_Rect r = { 20, y_offset, text_surface->w, text_surface->h };
            SDL_BlitSurface(text_surface, 0, surface, &r);
            SDL_FreeSurface(text_surface);
            y_offset += text_surface->h + 10;
        }
    }
}

SDL_Surface * load_cover(std::string rel_song_dir_path)
{
    std::string const abs_cover_path = cover_abs_path(rel_song_dir_path);

    for (auto const & name : names)
    {
        for (auto const & ext : exts)
        {
            std::string cover_path = abs_cover_path + name + "." + ext;
            return IMG_Load(cover_path.c_str());
        }
    }

    return nullptr;
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

struct gui_event_info
{
    bool mouse_event;

    int x;
    int y;

    bool pressed;
    // left mouse button was pressed before
    bool released;

    int last_x;
    int last_y;

    bool valid_swipe;
    std::chrono::steady_clock::time_point up_time_point;
    std::chrono::steady_clock::time_point last_swipe_time_point;

    unsigned int abs_xdiff;
    unsigned int abs_ydiff;
    int xdiff;
    int ydiff;
};

void apply_sdl_event(SDL_Event & e, gui_event_info & gei)
{

    if (e.type == SDL_MOUSEBUTTONDOWN)
    {
        if (!gei.pressed)
        {
            gei.x = e.button.x;
            gei.y = e.button.y;

            gei.pressed = true;
        }
        gei.mouse_event = true;
        gei.valid_swipe = false;
        std::cout << "down" << std::endl;
    }
    else if (e.type == SDL_MOUSEBUTTONUP)
    {
        gei.mouse_event = true;
        gei.pressed = false;
        gei.released = true;
        gei.last_x = gei.x;
        gei.last_y = gei.y;
        gei.x = e.button.x;
        gei.y = e.button.y;

        gei.up_time_point = std::chrono::steady_clock::now();
        gei.xdiff = gei.x - gei.last_x;
        gei.ydiff = gei.y - gei.last_y;
        gei.abs_xdiff = std::abs(gei.xdiff);
        gei.abs_ydiff = std::abs(gei.ydiff);
        gei.valid_swipe = gei.abs_xdiff > SWIPE_THRESHOLD_LOW_X || gei.abs_ydiff > SWIPE_THRESHOLD_LOW_Y;
        if (gei.valid_swipe)
            gei.last_swipe_time_point = std::chrono::steady_clock::now();
        std::cout << "up" << std::endl;
    }
    else
    {
        std::cout << "other" << std::endl;
        gei.mouse_event = false;
    }
}

bool within_rect(int x, int y, SDL_Rect const & r)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

bool pressed_and_let_go_in(SDL_Rect const & r, gui_event_info const & gei)
{
    // both clicks lie within r
    return within_rect(gei.x, gei.y, r) && within_rect(gei.last_x, gei.last_y, r)
                                        && gei.released;
}

bool button( SDL_Rect const & box
           , SDL_Surface * isurf
           , SDL_Surface * psurf
           , gui_event_info const & gei
           , SDL_Surface * screen
           )
{
    bool pressed = pressed_and_let_go_in(box, gei);

    SDL_Surface * surf = pressed ? psurf : isurf;

    // use LowerBlit ?
    SDL_BlitSurface(surf, nullptr, screen, const_cast<SDL_Rect *>(&box));

    return pressed;
}


// TODO better name
enum class action
{
    INC_VOLUME,
    DEC_VOLUME,
    NEXT_SONG,
    PREV_SONG,
    TOGGLE_PAUSE,
    NONE
};

action swipe_area(SDL_Rect const & box, gui_event_info const & gei)
{
    if (gei.mouse_event && within_rect(gei.x, gei.y, box) && gei.released)
    {
        std::cout << "recognized press" << std::endl;
        // swipe detection
        if (gei.valid_swipe)
        {
            std::cout << "recognized valid swipe with xdiff = " << gei.xdiff << ", ydiff = " << gei.ydiff << std::endl;
            // y is volume
            if (gei.abs_ydiff * DIR_UNAMBIG_FACTOR_THRESHOLD >= gei.abs_xdiff)
            {
                if (gei.ydiff < 0)
                {
                    return action::INC_VOLUME;
                }
                else
                {
                    return action::DEC_VOLUME;
                }
            }
            // x is song
            else if (gei.abs_xdiff * DIR_UNAMBIG_FACTOR_THRESHOLD >= gei.abs_ydiff)
            {
                if (gei.xdiff > 0)
                {
                    return action::NEXT_SONG;
                }
                else
                {
                    return action::PREV_SONG;
                }
            }
        }
        // check if the finger didn't move a lot and whether we're not doing a swipe motion directly before
        else if (    gei.abs_xdiff < TOUCH_DISTANCE_THRESHOLD_HIGH
                  && gei.abs_ydiff < TOUCH_DISTANCE_THRESHOLD_HIGH
                  && std::chrono::duration_cast<std::chrono::milliseconds>(gei.up_time_point - gei.last_swipe_time_point).count()
                     > SWIPE_WAIT_DEBOUNCE_MS_THRESHOLD
                )
        {
            return action::TOGGLE_PAUSE;
        }
    }

    return action::NONE;
}

int main(int argc, char * argv[])
{

    // TODO move to config
    // determines the minimum length of a swipe

    char const * const DEFAULT_FONT_PATH = "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf";

    // allows swipes with multiple lines, as long as the time between them is below this TODO not implemented
    //unsigned int const TOUCH_DEBOUNCE_TIME_MS_THRESHOLD_MAX = 200;

    std::mutex new_song_mailbox_mutex;
    std::queue<std::string> new_song_mailbox;

    mpd_control mpdc(
        [&](std::string const & uri)
        {
            scoped_lock lock(new_song_mailbox_mutex);
            new_song_mailbox.push(uri);
        }
    );

    std::thread mpdc_thread(&mpd_control::run, std::ref(mpdc));

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
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

    //TTF_SetFontStyle(font, TTF_STYLE_BOLD);

    // create window
    SDL_Window * window = SDL_CreateWindow
        ( "mpc-touch-lcd-gui"
        , SDL_WINDOWPOS_UNDEFINED
        , SDL_WINDOWPOS_UNDEFINED
#ifndef __arm__
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

    bool run = true;
    std::string last_song_dir_path;
    while (run)
    {

        SDL_Event ev;


        //std::chrono::steady_clock::time_point down_time_point = std::chrono::steady_clock::now();

        gui_event_info gei;
        gei.pressed = false;
        gei.released = false;
        gei.mouse_event = false;

        while (SDL_PollEvent(&ev) == 1)
        {
            if (ev.type == SDL_QUIT)
            {
                std::cout << "Requested quit" << std::endl;
                run = false;
            }
            else if (ev.type != SDL_MOUSEMOTION)
            {
                apply_sdl_event(ev, gei);



                action a = swipe_area({20, 0, screen->w - 20, screen->h}, gei);

                std::cout << (int)a << std::endl;
                switch (a)
                {
                    case action::INC_VOLUME:
                        mpdc.inc_volume(5);
                        break;
                    case action::DEC_VOLUME:
                        mpdc.dec_volume(5);
                        break;
                    case action::NEXT_SONG:
                        mpdc.next_song();
                        break;
                    case action::PREV_SONG:
                        mpdc.prev_song();
                        break;
                    case action::TOGGLE_PAUSE:
                        mpdc.toggle_pause();
                        break;
                    default:
                        break;
                }
            }
        }
        {
            std::unique_lock<std::mutex> lock(new_song_mailbox_mutex);

            if (!new_song_mailbox.empty())
            {
                std::string new_song_path = new_song_mailbox.front();
                new_song_mailbox.pop();

                lock.unlock();

                std::string new_song_dir_path = basename(new_song_path);

                // redraw cover if it is a new one
                if (new_song_dir_path != last_song_dir_path)
                {
                    SDL_Surface * cover_surface = load_cover(new_song_dir_path);
                    SDL_Rect brect = {20, 0, screen->w - 20, screen->h};
                    if (cover_surface != nullptr)
                    {
                        blit_preserve_ar(cover_surface, screen, &brect);
                    }
                    else
                    {
                        draw_cover_replacement( screen
                                              , brect
                                              , font
                                              , mpdc.get_current_title().get()
                                              , mpdc.get_current_artist().get()
                                              , mpdc.get_current_album().get()
                                              );
                    }
                    SDL_UpdateWindowSurface(window);

                    last_song_dir_path.swap(new_song_dir_path);
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    mpdc.stop();
    mpdc_thread.join();

    TTF_CloseFont(font);
    TTF_Quit();

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

