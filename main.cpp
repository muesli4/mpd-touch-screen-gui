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

void draw_cover(std::string rel_song_dir_path, SDL_Surface * surface, SDL_Window * window)
{
    // TODO keep picture ratio
    SDL_Rect screen_pos = { 40, 0, 240, 240 };
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
                SDL_Rect cover_rect = { 0, 0, cover->w, cover->h };
                SDL_BlitScaled(cover, &cover_rect, surface, &screen_pos);
                goto exit;
            }
        }
    }

    SDL_FillRect(surface, 0, SDL_MapRGB(surface->format, 0, 0, 0));
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

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window * window = SDL_CreateWindow
        ( "cover"
        , SDL_WINDOWPOS_UNDEFINED
        , SDL_WINDOWPOS_UNDEFINED
        , 320
        , 240
#ifndef __arm__
        , 0
#else
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
                    draw_cover(new_song_dir_path, screen, window);

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

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

