// SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "text_cover_provider.hpp"

bool text_cover_provider::update_cover(cover_updatable & u, song_data_provider const & p) const
{
    auto info = p.get_song_info();
    u.update_cover_from_song_info(info);
    return true;
}
