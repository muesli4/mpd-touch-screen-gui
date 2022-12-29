// SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <cstring>
#include "byte_buffer.hpp"

byte_buffer::byte_buffer()
    : _size(0)
    , _data(0)
{
}

byte_buffer::byte_buffer(std::byte * p, size_t size)
    : _size(size)
    , _data(p)
{
}

byte_buffer::byte_buffer(std::unique_ptr<std::byte[]> && p, size_t size)
    : _size(size)
    , _data(std::move(p))
{
}

byte_buffer::byte_buffer(size_t size)
    : _size(size)
    , _data(new std::byte[size])
{
}

byte_buffer::byte_buffer(byte_buffer const & other)
    : byte_buffer(other.size())
{
    std::memcpy(_data.get(), other.data(), other.size());
}

std::byte * byte_buffer::data()
{
    return _data.get();
}

std::byte const * byte_buffer::data() const
{
    return _data.get();
}

size_t byte_buffer::size() const
{
    return _size;
}

void byte_buffer::resize(size_t new_size)
{
    auto new_data = unique_byte_array_ptr(new std::byte[new_size]);

    std::memcpy(new_data.get(), _data.get(), _size);
    _size = new_size;
    _data.swap(new_data);
}

void byte_buffer::resize_double()
{
    resize(_size * 2);
}

