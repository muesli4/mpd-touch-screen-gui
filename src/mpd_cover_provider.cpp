// SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "mpd_cover_provider.hpp"

mpd_cover_provider::mpd_cover_provider(mpd_control & mpd_control, mpd_cover_type type)
    : _mpd_control(mpd_control)
    , _type(type)
{
}

bool mpd_cover_provider::update_cover(cover_updatable & u, song_data_provider const & p) const
{
    auto path = p.get_song_path();
    auto opt_image_data =
        _type == MPD_COVER_TYPE_ALBUMART ? _mpd_control.get_albumart(path)
                                         : _mpd_control.get_readpicture(path);
    bool found = opt_image_data.has_value();

    if (found)
    {
        u.update_cover_from_image_data(opt_image_data.value());
    }

    return found;
}
