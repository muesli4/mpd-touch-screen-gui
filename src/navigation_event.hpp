// SPDX-FileCopyrightText: 2022 Doron Behar
// SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef NAVIGATION_EVENT_HPP
#define NAVIGATION_EVENT_HPP

#include <libwtk-sdl2/navigation_type.hpp>
#include <SDL2/SDL_events.h>

#include "user_event.hpp"

enum class navigation_event_type
{
    NAVIGATION,
    ACTIVATE,
    SCROLL_UP,
    SCROLL_DOWN,
    MENU
};

struct navigation_event
{
    navigation_event_type type;

    // only valid with type == NAVIGATION
    navigation_type nt;
};

struct navigation_event_sender
{
    void push(navigation_event ne) const;
    void read(SDL_Event const & e, navigation_event & ne) const;

    bool is_event_type(uint32_t event_type) const;

    private:

    generic_user_event_sender _gues;
};

#endif
