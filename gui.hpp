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
    gui_context(
        gui_event_info const & gei,
        SDL_Surface * s,
        double dir_unambig_factor_threshold,
        unsigned int touch_distance_threshold_high,
        unsigned int swipe_wait_debounce_ms_threshold
    )
        : gei(gei)
        , target_surface(s)
        , dir_unambig_factor_threshold(dir_unambig_factor_threshold)
        , touch_distance_threshold_high(touch_distance_threshold_high)
        , swipe_wait_debounce_ms_threshold(swipe_wait_debounce_ms_threshold)
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
    double dir_unambig_factor_threshold;
    unsigned int touch_distance_threshold_high;
    unsigned int swipe_wait_debounce_ms_threshold;

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

bool within_rect(int x, int y, SDL_Rect const & r)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

bool pressed_and_released_in(SDL_Rect box, gui_event_info const & gei)
{
    // both clicks lie within box
    return !gei.pressed && within_rect(gei.event_x, gei.event_y, box) && within_rect(gei.down_x, gei.down_y, box);
    // TODO check whether the previous one was a mouse down event
}

bool pressed_in(SDL_Rect box, gui_event_info const & gei)
{
    return gei.pressed && within_rect(gei.event_x, gei.event_y, box);
}

bool is_button_active(SDL_Rect box, gui_event_info const & gei)
{
    return gei.mouse_event && pressed_and_released_in(box, gei);
}

bool text_button( SDL_Rect box
                , std::string text
                , font_atlas & fa
                , gui_context & gc
                )
{
    gc.draw_button_box(box, pressed_in(box, gc.gei));
    if (!text.empty())
        gc.draw_button_text(box, text, fa);

    return is_button_active(box, gc.gei);
}

bool image_button( SDL_Rect box
                 , SDL_Surface * idle_surf
                 , SDL_Surface * pressed_surf
                 , gui_context & gc
                 )
{
    SDL_BlitSurface(pressed_in(box, gc.gei) ? pressed_surf : idle_surf, nullptr, gc.target_surface, &box);
    return is_button_active(box, gc.gei);
}

bool swipe_is_press(gui_context const & gc)
{
    gui_event_info const & gei = gc.gei;
    // a touch display is inaccurate, a press with a finger might be interpreted as swipe
    return gei.abs_xdiff < gc.touch_distance_threshold_high
           && gei.abs_ydiff < gc.touch_distance_threshold_high
           && std::chrono::duration_cast<std::chrono::milliseconds>(
                  gei.up_time_point - gei.last_swipe_time_point
              ).count() > gc.swipe_wait_debounce_ms_threshold;
}

enum class swipe_action
{
    SWIPE_UP,
    SWIPE_DOWN,
    SWIPE_LEFT,
    SWIPE_RIGHT,
    PRESS,
    NONE
};

swipe_action swipe_area(SDL_Rect const & box, gui_context const & gc)
{
    gui_event_info const & gei = gc.gei;
    if (gei.mouse_event && within_rect(gei.down_x, gei.down_y, box) && !gei.pressed)
    {
        // swipe detection
        if (gei.valid_swipe)
        {
            if (gei.abs_ydiff * gc.dir_unambig_factor_threshold >= gei.abs_xdiff)
            {
                if (gei.ydiff < 0)
                {
                    return swipe_action::SWIPE_UP;
                }
                else
                {
                    return swipe_action::SWIPE_DOWN;
                }
            }
            else if (gei.abs_xdiff * gc.dir_unambig_factor_threshold >= gei.abs_ydiff)
            {
                if (gei.xdiff > 0)
                {
                    return swipe_action::SWIPE_RIGHT;
                }
                else
                {
                    return swipe_action::SWIPE_LEFT;
                }
            }
        }
        // check if the finger didn't move a lot and whether we're not doing a
        // swipe motion directly before
        else if (swipe_is_press(gc))
        {
            return swipe_action::PRESS;
        }
    }

    return swipe_action::NONE;
}

// produce boxes to equally space widgets horizontally
struct base_layout
{
    base_layout(unsigned int n, unsigned int spacing, SDL_Rect const & outer_box, int side)
        : _spacing(spacing)
        , _box_len(static_cast<double>(side - (n - 1) * spacing) / n)
        , _box_num(0)
        , _outer_box(outer_box)
    {
    }

    void next(int n = 1)
    {
        _box_num += n;
    }

    protected:

    int _spacing;
    double _box_len;
    double _box_num;
    SDL_Rect _outer_box;
};

struct v_layout : base_layout
{
    v_layout(unsigned int n, unsigned int spacing, SDL_Rect const & outer_box)
        : base_layout(n, spacing, outer_box, outer_box.h)
    {
    }

    SDL_Rect box(int n = 1)
    {
        int const h = static_cast<int>(_box_len * n) + _spacing * (n - 1);
        int const y = _outer_box.y + _box_num * (_box_len + _spacing);
        SDL_Rect res { _outer_box.x, y, _outer_box.w, h };
        return res;
    }
};

struct h_layout : base_layout
{
    h_layout(unsigned int n, unsigned int spacing, SDL_Rect const & outer_box)
        : base_layout(n, spacing, outer_box, outer_box.w)
    {
    }

    SDL_Rect box(int n = 1)
    {
        int const w = static_cast<int>(_box_len * n) + _spacing * (n - 1);
        int const x = _outer_box.x + _box_num * (_box_len + _spacing);
        SDL_Rect res { x, _outer_box.y, w, _outer_box.h };
        return res;
    }
};

SDL_Rect pad_box(SDL_Rect box, int padding)
{
    return { box.x + padding, box.y + padding, box.w - 2 * padding, box.h - 2 * padding };
}

