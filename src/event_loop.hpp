#pragma once

#include <SDL2/SDL.h>

#include "program_config.hpp"

enum quit_action
{
    SHUTDOWN,
    REBOOT,
    NONE
};

bool idle_timer_enabled(program_config const & cfg);

quit_action event_loop(SDL_Renderer * renderer, program_config const & cfg);
