// SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef COVER_PROVIDER_HPP
#define COVER_PROVIDER_HPP

#include <string>

#include "cover_updatable.hpp"
#include "song_data_provider.hpp"

struct cover_provider
{
    // Try to update the cover and return whether it succeeded.
    virtual bool update_cover(cover_updatable & u, song_data_provider const & p) const = 0;
};

#endif

