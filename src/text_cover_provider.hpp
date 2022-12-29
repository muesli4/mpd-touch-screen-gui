// SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef TEXT_COVER_PROVIDER_HPP
#define TEXT_COVER_PROVIDER_HPP

#include "cover_provider.hpp"

struct text_cover_provider : cover_provider
{
    bool update_cover(cover_updatable & u, song_data_provider const & p) const;
};

#endif

