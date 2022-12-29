// SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef SONG_DATA_PROVIDER_HPP
#define SONG_DATA_PROVIDER_HPP

#include <string>

#include "song_info.hpp"

struct song_data_provider
{
    virtual song_info get_song_info() const = 0;
    virtual std::string get_song_path() const = 0;
};

#endif

