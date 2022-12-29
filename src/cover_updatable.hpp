// SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef COVER_UPDATABLE_HPP
#define COVER_UPDATABLE_HPP

#include <vector>
#include <cstddef>

#include <libwtk-sdl2/sdl_util.hpp>

#include "song_info.hpp"

struct cover_updatable
{
    virtual void update_cover_from_local_file(std::string filename) = 0;
    virtual void update_cover_from_song_info(song_info const & info) = 0;
    virtual void update_cover_from_image_data(byte_array_slice data) = 0;
};

#endif

