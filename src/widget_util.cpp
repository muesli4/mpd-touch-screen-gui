#include "widget_util.hpp"

#include <libwtk-sdl2/box.hpp>
#include <libwtk-sdl2/text_button.hpp>
#include <libwtk-sdl2/padding.hpp>
#include <libwtk-sdl2/texture_button.hpp>

widget_ptr make_texture_button(SDL_Renderer * renderer, std::string filename, std::function<void()> callback)
{
    return std::make_shared<texture_button>(load_shared_texture_from_image(renderer, filename), callback);
}

widget_ptr add_list_view_controls(SDL_Renderer * r, std::shared_ptr<list_view> lv, std::string left_filename, std::function<void()> left_action)
{
    return vbox({ { true, lv }
                , { false, hbox({ { false, make_texture_button(r, left_filename, left_action) }
                                , { false, make_texture_button(r, ICONDIR "scroll_up.png", [=](){ lv->scroll_up(); }) }
                                , { false, make_texture_button(r, ICONDIR "scroll_down.png", [=](){ lv->scroll_down(); }) }
                                }, 5, true) }
                }, 5, false);
}

