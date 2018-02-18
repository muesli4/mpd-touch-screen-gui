#include "song_list_view.hpp"

#include <libwtk-sdl2/box.hpp>
#include <libwtk-sdl2/button.hpp>
#include <libwtk-sdl2/padding.hpp>

widget_ptr song_list_view(std::shared_ptr<list_view> lv, std::string left_label, std::function<void()> left_callback)
{
    return vbox({ { true, lv }
                , { false, hbox({ { false, std::make_shared<button>(left_label, left_callback) }
                                , { false, std::make_shared<button>("▲", [=](){ lv->scroll_up(); }) }
                                , { false, std::make_shared<button>("▼", [=](){ lv->scroll_down(); }) }
                                }, 5, true) }
                }, 5, false);
}

/*
song_list_view::song_list_view(std::shared_ptr<list_view> lv, std::function<void()> left_callback, std::string left_label)
    : embedded_widget({ lv, make_controls(lv, left_callback, left_label) })
{
}

widget_ptr song_list_view::make_controls(std::shared_ptr<list_view> lv, std::function<void()> left_callback, std::string left_label)
{
    return hbox({ std::make_shared<button>(left_label, left_callback)
                , std::make_shared<button>("▲", std::bind(lv, list_view::scroll_up))
                , std::make_shared<button>("▼", std::bind(lv, list_view::scroll_down))
                });
}
*/
