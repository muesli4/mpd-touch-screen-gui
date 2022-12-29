// SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef BYTE_BUFFER_HPP
#define BYTE_BUFFER_HPP

#include <cstddef>
#include <memory>

struct byte_buffer
{
    byte_buffer();
    byte_buffer(std::byte * p, size_t size);
    byte_buffer(std::unique_ptr<std::byte[]> && p, size_t size);
    explicit byte_buffer(size_t size);
    byte_buffer(byte_buffer const & other);

    byte_buffer(byte_buffer && other) = default;
    byte_buffer & operator=(byte_buffer && other) = default;

    std::byte * data();
    std::byte const * data() const;

    size_t size() const;

    void resize(size_t new_size);
    void resize_double();

    private:

    typedef std::unique_ptr<std::byte[]> unique_byte_array_ptr;

    size_t _size;

    unique_byte_array_ptr _data;
};

#endif

