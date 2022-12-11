// SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "player_mpd_model.hpp"

player_mpd_model::player_mpd_model(mpd_control & c)
    : _mpd_control(c)
    , _quit_action()
{
}

void player_mpd_model::inc_volume(unsigned int volume_step)
{
    _mpd_control.inc_volume(volume_step);
}

void player_mpd_model::dec_volume(unsigned int volume_step)
{
    _mpd_control.dec_volume(volume_step);
}

void player_mpd_model::next_song()
{
    _mpd_control.next_song();
}

void player_mpd_model::prev_song()
{
    _mpd_control.prev_song();
}

void player_mpd_model::toggle_pause()
{
    _mpd_control.toggle_pause();
}

void player_mpd_model::toggle_random()
{
    _mpd_control.toggle_random();
}

void player_mpd_model::play_position(std::size_t pos)
{
    _mpd_control.play_position(pos);
}

void player_mpd_model::shutdown()
{
    _quit_action = quit_action::SHUTDOWN;
}

void player_mpd_model::reboot()
{
    _quit_action = quit_action::REBOOT;
}

void player_mpd_model::quit()
{
    _quit_action = quit_action::NONE;
}

bool player_mpd_model::is_finished() const
{
    return _quit_action.has_value();
}

quit_action player_mpd_model::get_quit_action() const
{
    return _quit_action.value_or(quit_action::NONE);
}

