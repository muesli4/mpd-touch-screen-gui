// SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef FILESYSTEM_COVER_PROVIDER
#define FILESYSTEM_COVER_PROVIDER

#include "cover_provider.hpp"
#include "program_config.hpp"

struct filesystem_cover_provider : cover_provider
{
    filesystem_cover_provider(cover_config const & config);

    bool update_cover(cover_updatable & u, song_data_provider const & p) const;

    private:

    cover_config const & _config;
};

#endif

