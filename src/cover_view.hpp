// SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef COVER_VIEW_HPP
#define COVER_VIEW_HPP

#include <libwtk-sdl2/embedded_widget.hpp>
#include <libwtk-sdl2/notebook.hpp>
#include <libwtk-sdl2/texture_view.hpp>
#include <libwtk-sdl2/label.hpp>
#include <libwtk-sdl2/swipe_area.hpp>

enum class cover_type
{
    IMAGE,
    SONG_INFO
};

struct cover_view : embedded_widget<notebook>
{
    cover_view(std::function<void(swipe_direction)> swipe_callback, std::function<void()> press_callback);
    cover_view(std::shared_ptr<label> l, std::shared_ptr<texture_view> tv, std::function<void(swipe_direction)> swipe_callback, std::function<void()> press_callback);
    ~cover_view() override;

    void on_mouse_up_event(mouse_up_event const & e) override;

    std::vector<widget *> get_children() override;
    std::vector<widget const *> get_children() const override;

    void on_box_allocated() override;

    void set_cover(unique_texture_ptr texture_ptr);
    void set_cover(std::string title, std::string artist, std::string album);

    private:

    std::shared_ptr<label> _label;
    std::shared_ptr<texture_view> _texture_view;
    swipe_area _swipe_area;
};

#endif

