// SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

struct player_model
{
    virtual void inc_volume(unsigned int volume_step) = 0;
    virtual void dec_volume(unsigned int volume_step) = 0;
    virtual void next_song() = 0;
    virtual void prev_song() = 0;
    virtual void toggle_pause() = 0;
    virtual void toggle_random() = 0;

    virtual void play_position(std::size_t pos) = 0;

    virtual void shutdown() = 0;
    virtual void reboot() = 0;
};
