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
#include "font_atlas.hpp"

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

void show_rect(SDL_Rect const & r)
{
    std::cout << r.x << " " << r.y << " on " << r.w << "x" << r.h << std::endl;
}

// blit a surface to another surface while preserving aspect ratio
void blit_preserve_ar(SDL_Surface * source, SDL_Surface * dest, SDL_Rect const * destrect)
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

SDL_Surface * create_similar_surface(SDL_Surface const * s, int width, int height)
{
    SDL_PixelFormat const & format = *(s->format);
    return SDL_CreateRGBSurface(0, width, height, format.BitsPerPixel, format.Rmask, format.Gmask, format.Bmask, format.Amask);
}

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

// ----------------------------------------------------------------------------
// Drawing simple absolutely placed gui elements in an event loop.
//

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

// TODO add as method to class
void apply_sdl_event(SDL_Event & e, gui_event_info & gei)
{
    if (e.type == SDL_MOUSEBUTTONDOWN)
    {
        gei.mouse_event = true;
        gei.pressed = true;
        gei.event_x = e.button.x;
        gei.event_y = e.button.y;
        gei.valid_swipe = false;
    }
    else if (e.type == SDL_MOUSEBUTTONUP)
    {
        gei.mouse_event = true;
        gei.pressed = false;
        gei.down_x = gei.event_x;
        gei.down_y = gei.event_y;
        gei.event_x = e.button.x;
        gei.event_y = e.button.y;

        // FIXME use SDL timestamps? this may be 100ms off
        gei.up_time_point = std::chrono::steady_clock::now();
        gei.xdiff = gei.event_x - gei.down_x;
        gei.ydiff = gei.event_y - gei.down_y;
        gei.abs_xdiff = std::abs(gei.xdiff);
        gei.abs_ydiff = std::abs(gei.ydiff);
        gei.valid_swipe = gei.abs_xdiff > SWIPE_THRESHOLD_LOW_X || gei.abs_ydiff > SWIPE_THRESHOLD_LOW_Y;
        if (gei.valid_swipe)
            gei.last_swipe_time_point = std::chrono::steady_clock::now();
    }
    else
    {
        gei.mouse_event = false;
    }
}

bool swipe_is_press(gui_event_info const & gei)
{
    // a touch display is inaccurate, a press with a finger might be interpreted as swipe
    return gei.abs_xdiff < TOUCH_DISTANCE_THRESHOLD_HIGH
           && gei.abs_ydiff < TOUCH_DISTANCE_THRESHOLD_HIGH
           && std::chrono::duration_cast<std::chrono::milliseconds>(
                  gei.up_time_point - gei.last_swipe_time_point
              ).count() > SWIPE_WAIT_DEBOUNCE_MS_THRESHOLD;
}

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

// check overflow and ensure at most upper_bound
unsigned int inc_ensure_upper(unsigned int new_pos, unsigned int old_pos, unsigned int upper_bound)
{
    return new_pos < old_pos ? upper_bound : std::min(new_pos, upper_bound);
}

// check underflow and ensure at least lower_bound
unsigned int dec_ensure_lower(unsigned int new_pos, unsigned int old_pos, unsigned int lower_bound)
{
    return new_pos > old_pos ? lower_bound : std::max(new_pos, lower_bound);
}

unsigned int list_view_visible_items(SDL_Rect box, font_atlas & fa)
{
    return (box.h - 2) / fa.font_line_skip();
}

// for the moment only one highlight is supported, maybe more make sense later on
int list_view(SDL_Rect box, std::vector<std::string> const & entries, unsigned int & pos, int highlight, font_atlas & fa, gui_context & gc)
{
    unsigned int const visible_items = list_view_visible_items(box, fa);

    int selection = -1;
    gc.draw_entry_box(box);

    SDL_Rect text_box { box.x + 1, box.y + 1, box.w - 2, static_cast<int>(fa.height()) };

    gui_event_info const & gei = gc.gei;

    bool const swipe = gei.valid_swipe && gei.mouse_event && within_rect(gei.down_x, gei.down_y, box) && !gei.pressed;
    // hacky update before drawing...
    if (swipe && gei.abs_ydiff * DIR_UNAMBIG_FACTOR_THRESHOLD >= gei.abs_xdiff)
    {
        int const distance = static_cast<int>(visible_items) * 10 * gei.ydiff / (box.w / 2);
        unsigned int const next_pos = pos + distance;
        if (gei.ydiff < 0)
            pos = dec_ensure_lower(next_pos, pos, 0);
        else
            pos = inc_ensure_upper(next_pos, pos, entries.size() < visible_items ? 0 : entries.size() - visible_items);
    }

    std::size_t n = pos;
    while (n < entries.size() && text_box.y < box.y + box.h)
    {
        int const overlap = (text_box.y + text_box.h) - (box.y + box.h);
        int const h = text_box.h - (overlap < 0 ? 0 : overlap + 1);

        SDL_Rect src_rect { 0, 0, text_box.w, h };
        SDL_Rect abs_rect {text_box.x, text_box.y, text_box.w, h};

        // favor pressed over active
        if (pressed_in(abs_rect, gei))
            gc.draw_entry_pressed_background(abs_rect);
        else if (highlight == static_cast<int>(n))
            gc.draw_entry_active_background(abs_rect);

        auto text_surf_ptr = fa.text(entries[n]);
        SDL_SetSurfaceColorMod(text_surf_ptr.get(), 0, 0, 0);

        SDL_Rect tmp_rect = text_box;
        SDL_BlitSurface(text_surf_ptr.get(), &src_rect, gc.target_surface, &tmp_rect);

        if (is_button_active(abs_rect, gei))
            selection = n;

        text_box.y += fa.font_line_skip();
        n++;
    }

    // draw position indicator if it doesn't fit on one page
    if (entries.size() > visible_items)
    {
        int const ind_len = std::max(15, static_cast<int>(((box.h - 2) * visible_items) / entries.size()));
        int const ind_w = 7;

        int const ind_y = box.y + 1 + ((box.h - 2 - ind_len) * pos) / (entries.size() - visible_items);
        SDL_Rect ind_rect { box.x + box.w - ind_w - 1, ind_y, ind_w, ind_len};
        gc.draw_entry_position_indicator(ind_rect);
    }

    return swipe ? -1 : selection;
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
    if (gei.mouse_event && within_rect(gei.down_x, gei.down_y, box) && !gei.pressed)
    {
        // swipe detection
        if (gei.valid_swipe)
        {
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
        else if (swipe_is_press(gei))
        {
            return action::TOGGLE_PAUSE;
        }
    }

    return action::NONE;
}

void handle_action(action a, mpd_control & mpdc, unsigned int volume_step)
{
    switch (a)
    {
        case action::INC_VOLUME:
            mpdc.inc_volume(volume_step);
            break;
        case action::DEC_VOLUME:
            mpdc.dec_volume(volume_step);
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

enum quit_action
{
    SHUTDOWN,
    REBOOT,
    NONE
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

    std::unique_ptr<SDL_Surface, void(*)(SDL_Surface *)> cover_surface_ptr{nullptr, [](SDL_Surface * s) {SDL_FreeSurface(s);}};
    bool has_cover = false;

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
        gui_context gc(gei, screen);

        bool run = true;
        while (run)
        {
            SDL_Event ev;

            while (SDL_PollEvent(&ev) == 1)
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
                                // if (has_cover && same album)
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
                    apply_sdl_event(ev, gei);

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
                        action a = swipe_area(view_rect, gei);

                        // redraw cover if it is a new one or if marked dirty
                        if (view_dirty || !cover_surface_ptr)
                        {
                            if (!cover_surface_ptr)
                            {
                                SDL_Rect cover_rect { 0, 0, view_rect.w, view_rect.h};
                                SDL_Surface * cover_surface;
                                view_dirty = true;
                                SDL_Surface * img_surface = load_cover(current_song_path);
                                if (img_surface != nullptr)
                                {
                                    has_cover = true;
                                    cover_surface = create_similar_surface(screen, cover_rect.w, cover_rect.h);
                                    blit_preserve_ar(img_surface, cover_surface, &cover_rect);
                                }
                                else
                                {
                                    has_cover = false;
                                    cover_surface =
                                        create_cover_replacement( screen
                                                                , cover_rect
                                                                , fa
                                                                , mpdc.get_current_title()
                                                                , mpdc.get_current_artist()
                                                                , mpdc.get_current_album()
                                                                );
                                }
                                cover_surface_ptr.reset(cover_surface);
                            }
                            SDL_Rect r = view_rect;
                            SDL_BlitSurface(cover_surface_ptr.get(), nullptr, screen, &r);
                            SDL_UpdateWindowSurfaceRects(window, &view_rect, 1);
                            view_dirty = false;
                        }
                        handle_action(a, mpdc, 5);
                    }
                    else
                    {
                        if (current_view == view_type::SHUTDOWN)
                        {
                            gc.draw_background(view_rect);
                            //SDL_FillRect(screen, &view_rect, SDL_MapRGB(screen->format, 20, 200, 40));

                            v_layout l(2, 4, pad_box(view_rect, 44));

                            if (text_button(l.box(), "Shutdown", fa, gc))
                            {
                                run = false;
                                quit_action = quit_action::SHUTDOWN;
                            }
                            l.next();
                            if (text_button(l.box(), "Reboot", fa, gc))
                            {
                                run = false;
                                quit_action = quit_action::REBOOT;
                            }
                        }
                        else
                        {
                            gc.draw_background(view_rect);

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

                                // TODO calculcate from text height (font ascent)
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
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
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

