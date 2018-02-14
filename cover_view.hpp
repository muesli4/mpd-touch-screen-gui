#ifndef COVER_VIEW_HPP
#define COVER_VIEW_HPP

#include <libwtk-sdl2/embedded_widget.hpp>
#include <libwtk-sdl2/bin.hpp>
#include <libwtk-sdl2/texture_view.hpp>
#include <libwtk-sdl2/label.hpp>
#include <libwtk-sdl2/swipe_area.hpp>

enum class cover_type
{
    IMAGE,
    SONG_INFO
};

struct cover
{
    cover_type type;
    union
    {
        unique_texture_ptr texture_ptr;
        struct
        {
            std::string artist;
            std::string title;
            std::string album;
        } info;
    };
};

struct cover_view : bin
{
    cover_view(std::function<void(swipe_action)> swipe_callback, std::function<void()> press_callback);
    ~cover_view() override;

    void on_mouse_up_event(mouse_up_event const & e) override;

    void set_cover(cover c);

    private:

    swipe_area _swipe_area;
};

#endif

