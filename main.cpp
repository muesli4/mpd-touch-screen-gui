// poor mans mpd cover display (used for ILI9341 320x240 pixel display)

#include <iostream>
#include <cstring>
#include <functional>
#include <queue>
#include <array>

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

int const base_dir_len = sizeof(base_dir);

std::array<std::string const, 3> const exts = { "png", "jpeg", "jpg" };
std::array<std::string const, 3> const names = { "front", "cover", "back" };

void show_rect(SDL_Rect const & r)
{
    std::cout << r.x << " " << r.y << " on " << r.w << "x" << r.h << std::endl;
}

void draw_cover( std::string rel_song_dir_path
               , std::string title
               , std::string artist
               , std::string album
               , TTF_Font * font
               , SDL_Surface * surface
               , SDL_Window * window
               )
{
    std::string const abs_cover_path = std::string(base_dir) + rel_song_dir_path;

    SDL_Surface * cover = 0;
    for (auto & name : names)
    {
        for (auto & ext : exts)
        {
            std::string cover_path = abs_cover_path + name + "." + ext;
            cover = IMG_Load(cover_path.c_str());

            if (cover != 0)
            {
                // draw cover while retaining image ratio
                int const w = surface->w;
                int const h = surface->h;
                int const cw = cover->w;
                int const ch = cover->h;

                double const surface_ratio = static_cast<double>(w) / static_cast<double>(h);
                double const cover_ratio = static_cast<double>(cw) / static_cast<double>(ch);

                bool const surface_wider = surface_ratio >= cover_ratio;

                int const rw = surface_wider ? cover_ratio * h : w;
                int const rh = surface_wider ? h : (1 / cover_ratio) * w;

                int const pad_x = (w - rw) / 2;
                int const pad_y = (h - rh) / 2;

                SDL_Rect cover_rect = { 0, 0, cw, ch };
                SDL_Rect screen_pos = { pad_x, pad_y, rw, rh };
                SDL_BlitScaled(cover, &cover_rect, surface, &screen_pos);

                // fill remaining rects
                if (surface_wider)
                {
                    SDL_Rect left = { 0, 0, pad_x, h };
                    SDL_FillRect(surface, &left, SDL_MapRGB(surface->format, 0, 0, 0));

                    SDL_Rect right = { w - pad_x, 0, pad_x, h };
                    SDL_FillRect(surface, &right, SDL_MapRGB(surface->format, 0, 0, 0));
                }
                else
                {
                    SDL_Rect top = { 0, 0, w, pad_y };
                    SDL_FillRect(surface, &top, SDL_MapRGB(surface->format, 0, 0, 0));

                    SDL_Rect bottom = { 0, h - pad_y, w, pad_y };
                    SDL_FillRect(surface, &bottom, SDL_MapRGB(surface->format, 0, 0, 0));
                }

                goto exit;
            }
        }
    }

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
            }
            y_offset += text_surface->h + 10;
        }
    }

exit:
    SDL_UpdateWindowSurface(window);
}

int main(int argc, char * argv[])
{

    // TODO move to config
    unsigned int const MOVEMENT_THRESHOLD_X = 30;
    unsigned int const MOVEMENT_THRESHOLD_Y = MOVEMENT_THRESHOLD_X;

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

    TTF_Font * font = TTF_OpenFont("/usr/share/fonts/TTF/DejaVuSans-Bold.ttf", 20);

    if (font == 0)
    {
        std::cerr << "Could not load font:"
                  << TTF_GetError() << '.' << std::endl;
        std::exit(0);
    }

    //TTF_SetFontStyle(font, TTF_STYLE_BOLD);

    // create window
    SDL_Window * window = SDL_CreateWindow
        ( "cover"
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
    int x;
    int y;
    std::string last_song_dir_path;
    while (run)
    {

        SDL_Event ev;
        while (SDL_PollEvent(&ev) == 1)
        {
            if (ev.type == SDL_QUIT)
            {
                std::cout << "Requested quit" << std::endl;
                run = false;
            }
            //else if (ev.type == SDL_FINGERDOWN)
            else if (ev.type == SDL_MOUSEBUTTONDOWN)
            {
                x = ev.button.x;
                y = ev.button.y;
            }
            else if (ev.type == SDL_MOUSEBUTTONUP)
            {
                int xdiff = ev.button.x - x;
                int ydiff = ev.button.y - y;

                unsigned int abs_xdiff = std::abs(xdiff);
                unsigned int abs_ydiff = std::abs(ydiff);

                // swipe detection
                if (abs_xdiff > MOVEMENT_THRESHOLD_X || abs_ydiff > MOVEMENT_THRESHOLD_Y)
                {
                    // y is volume
                    if (abs_ydiff > abs_xdiff)
                    {
                        if (ydiff < 0)
                        {
                            mpdc.inc_volume(5);
                        }
                        else
                        {
                            mpdc.dec_volume(5);
                        }
                    }
                    // x is song
                    else
                    {
                        if (xdiff > 0)
                        {
                            mpdc.next_song();
                        }
                        else
                        {
                            mpdc.prev_song();
                        }
                    }
                }
                else
                {
                    mpdc.toggle_pause();
                }
            }
        }
        {
            new_song_mailbox_mutex.lock();

            if (!new_song_mailbox.empty())
            {
                std::string new_song_path = new_song_mailbox.front();
                new_song_mailbox.pop();

                new_song_mailbox_mutex.unlock();

                std::string new_song_dir_path = basename(new_song_path);

                // redraw cover if it is a new one
                if (new_song_dir_path != last_song_dir_path)
                {

                    draw_cover( new_song_dir_path
                              , mpdc.get_current_title().get()
                              , mpdc.get_current_artist().get()
                              , mpdc.get_current_album().get()
                              , font
                              , screen
                              , window
                              );

                    last_song_dir_path.swap(new_song_dir_path);
                    new_song_dir_path.clear();
                }
            }
            else
            {
                new_song_mailbox_mutex.unlock();
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

