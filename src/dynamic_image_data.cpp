// SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dynamic_image_data.hpp"

dynamic_image_data::dynamic_image_data(byte_buffer buffer, size_t size)
    : _buffer(std::move(buffer))
    , _size(size)
{
}

dynamic_image_data::operator byte_array_slice()
{
    return byte_array_slice(_buffer.data(), _size);
}
