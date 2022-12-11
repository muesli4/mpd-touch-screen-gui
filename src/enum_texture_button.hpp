// SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef ENUM_TEXTURE_BUTTON_HPP
#define ENUM_TEXTURE_BUTTON_HPP

#include <libwtk-sdl2/button.hpp>
#include <libwtk-sdl2/sdl_util.hpp>

template <int N>
std::array<unique_texture_ptr, N> load_texture_array_from_files(SDL_Renderer * r, std::array<std::string, N> filenames)
{
    std::array<unique_texture_ptr, N> result;
    for (int i = 0; i < N; ++i)
    {
        result[i] = load_texture_from_image(r, filenames[i]);
    }
    return result;
}

template <typename Enum, int N>
struct enum_texture_button : button
{
    typedef std::array<unique_texture_ptr, N> texture_ptr_array;

    enum_texture_button(texture_ptr_array && textures, Enum state, std::function<void()> callback)
        : button(callback)
        , _state(state)
        , _textures(std::move(textures))
    {
        mark_dirty();
    }

    ~enum_texture_button() override
    {
    }

    void set_state(Enum state)
    {
        if (_state != state)
        {
            _state = state;
            mark_dirty();
        }
    }

    private:

    SDL_Texture const * get_texture() const
    {
        return _textures[static_cast<int>(_state)].get();
    }

    void draw_drawable(draw_context & dc, rect box) const override
    {
        auto tex = get_texture();

    rect tex_box = center_vec_within_rect(texture_dim(tex), get_box());
        // TODO allow nullptr? how to prevent it?
        dc.copy_texture(const_cast<SDL_Texture *>(tex), tex_box);
    }

    vec get_drawable_size() const override
    {
        return texture_dim(get_texture());
    }

    // only local state for redraw
    Enum _state;

    texture_ptr_array _textures;
};

template <typename Enum, int N>
std::shared_ptr<enum_texture_button<Enum, N>> make_enum_texture_button(SDL_Renderer * renderer, Enum state, std::array<std::string, N> && filenames, std::function<void()> callback)
{
    return std::make_shared<enum_texture_button<Enum, N>>(load_texture_array_from_files<N>(renderer, std::move(filenames)), state, callback);
}

#endif

