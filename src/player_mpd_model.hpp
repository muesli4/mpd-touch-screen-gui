// SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <cstddef>

#include "player_model.hpp"

#include "mpd_control.hpp"
#include "quit_action.hpp"


struct player_mpd_model : player_model
{
    player_mpd_model(mpd_control & mpdc);

    void inc_volume(unsigned int volume_step);
    void dec_volume(unsigned int volume_step);
    void next_song();
    void prev_song();
    void toggle_pause();
    void toggle_random();

    void play_position(std::size_t pos);

    void shutdown();
    void reboot();
    void quit();

    bool is_finished() const;
    quit_action get_quit_action() const;

    private:

    mpd_control & _mpd_control;

    std::optional<quit_action> _quit_action;
};
