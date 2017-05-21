#pragma once

#include <chrono>

#include <SDL2/SDL.h>

struct gui_event_info
{
    gui_event_info()
        : mouse_event(false)
        , pressed(false)
    {}

    // TODO use enum
    // add a draw event that will only draw
    bool mouse_event;
    bool pressed;

    int event_x;
    int event_y;


    int down_x;
    int down_y;

    bool valid_swipe;
    std::chrono::steady_clock::time_point up_time_point;
    std::chrono::steady_clock::time_point last_swipe_time_point;

    unsigned int abs_xdiff;
    unsigned int abs_ydiff;
    int xdiff;
    int ydiff;
};


struct gui_context
{
    gui_context(gui_event_info const & gei, SDL_Surface * s)
        : gei(gei)
        , target_surface(s)
        , button_bg_color{25, 25, 25}//{230, 230, 230}
        , button_fg_color{235, 235, 235}//{20, 20, 20}
        , button_frame_color{105, 105, 105}//{150, 150, 150}
        , button_selected_bg_color{105, 55, 55}//{250, 200, 200}
        , entry_bg_color{255, 255, 255}
        , entry_frame_color{100, 100, 100}
        , entry_selected_bg_color{250, 200, 200}
        , bg_color{0, 0, 0} //250, 250, 250}
        , active_color{230, 230, 255}
    {
        renderer = SDL_CreateSoftwareRenderer(s);
    }

    ~gui_context()
    {
        SDL_DestroyRenderer(renderer);
    }

    void render()
    {
        SDL_RenderPresent(renderer);
    }

    void draw_button_box(SDL_Rect box, bool activated)
    {
        set_color(activated ? button_selected_bg_color : button_bg_color);
        SDL_RenderFillRect(renderer, &box);
        set_color(button_frame_color);
        SDL_RenderDrawRect(renderer, &box);
    }

    void draw_button_text(SDL_Rect box, std::string const & text, font_atlas & fa)
    {
        auto text_surf_ptr = fa.text(text);
        SDL_SetSurfaceColorMod(text_surf_ptr.get(), button_fg_color.r, button_fg_color.g, button_fg_color.b);
        // center text in box
        SDL_Rect target_rect = {box.x + (box.w - text_surf_ptr->w) / 2, box.y + (box.h - text_surf_ptr->h) / 2, text_surf_ptr->w, text_surf_ptr->h};
        SDL_BlitSurface(text_surf_ptr.get(), nullptr, target_surface, &target_rect);
    }

    void draw_entry_box(SDL_Rect box)
    {
        set_color(entry_bg_color);
        SDL_RenderFillRect(renderer, &box);
        set_color(entry_frame_color);
        SDL_RenderDrawRect(renderer, &box);
    }

    void draw_background(SDL_Rect box)
    {
        set_color(bg_color);
        SDL_RenderFillRect(renderer, &box);
    }

    void draw_entry_pressed_background(SDL_Rect box)
    {
        set_color(entry_selected_bg_color);
        SDL_RenderFillRect(renderer, &box);
    }

    void draw_entry_active_background(SDL_Rect box)
    {
        set_color(active_color);
        SDL_RenderFillRect(renderer, &box);
    }

    void draw_entry_position_indicator(SDL_Rect box)
    {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 150, 250, 150, 200);

        SDL_RenderFillRect(renderer, &box);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }

    gui_event_info const & gei;
    SDL_Surface * target_surface;
    SDL_Renderer * renderer;

    private:
    void set_color(SDL_Color c)
    {
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 0);
    }

    SDL_Color button_bg_color;
    SDL_Color button_fg_color;
    SDL_Color button_frame_color;
    SDL_Color button_selected_bg_color;
    SDL_Color entry_bg_color;
    SDL_Color entry_frame_color;
    SDL_Color entry_selected_bg_color;
    SDL_Color bg_color;

    // displays the status of something, but not related to direct user interaction
    SDL_Color active_color;
};

