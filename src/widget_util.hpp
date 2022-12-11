// SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <functional>

#include <libwtk-sdl2/list_view.hpp>

// Adds controls to a list view.
widget_ptr add_list_view_controls(SDL_Renderer * r, std::shared_ptr<list_view> lv, std::string left_filename, std::function<void()> left_action);

widget_ptr make_texture_button(SDL_Renderer * renderer, std::string filename, std::function<void()> callback);
