// SPDX-FileCopyrightText: 2022 Moritz Bruder <muesli4 at gmail dot com>
// SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef MPD_COVER_PROVIDER_HPP
#define MPD_COVER_PROVIDER_HPP

#include "cover_provider.hpp"
#include "mpd_control.hpp"

enum mpd_cover_type
{
    MPD_COVER_TYPE_ALBUMART,
    MPD_COVER_TYPE_READPICTURE
};

struct mpd_cover_provider : cover_provider
{
    mpd_cover_provider(mpd_control & mpd_control, mpd_cover_type type);

    bool update_cover(cover_updatable & u, song_data_provider const & p) const;

    private:

    mpd_control & _mpd_control;

    mpd_cover_type _type;
};

#endif

