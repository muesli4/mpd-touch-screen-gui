#pragma once

/*
#include <libwtk-sdl2/embedded_widget.hpp>
#include <libwtk-sdl2/box.hpp>
*/

#include <libwtk-sdl2/list_view.hpp>


// Adds controls to a list view.
widget_ptr song_list_view(std::shared_ptr<list_view> lv, std::string left_label, std::function<void()> left_callback);

/*
struct song_list_view : embedded_widget<box>
{
    song_list_view(std::shared_ptr<list_view> lv, std::function<void()> left_callback, std::string left_label);

    private:

    static widget_ptr make_controls(std::shared_ptr<list_view> lv, std::function<void()> left_callback, std::string left_label);

    std::shared_ptr<list_view> _list_view;
};
*/
