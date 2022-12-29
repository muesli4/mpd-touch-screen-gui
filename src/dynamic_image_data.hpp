// SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef DYNAMIC_IMAGE_DATA_HPP
#define DYNAMIC_IMAGE_DATA_HPP

#include <libwtk-sdl2/byte_array_slice.hpp>

#include "byte_buffer.hpp"

// Owns image data.
struct dynamic_image_data
{
    dynamic_image_data(byte_buffer buffer, size_t size);

    operator byte_array_slice();

    private:

    byte_buffer _buffer;
    size_t _size;
};

#endif
