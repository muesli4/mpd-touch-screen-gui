#pragma once

#include <SDL2/SDL.h>

#include "program_config.hpp"
#include "navigation_event.hpp"
#include "widget_context.hpp"

enum quit_action
{
    SHUTDOWN,
    REBOOT,
    NONE
};

bool idle_timer_enabled(program_config const & cfg);

struct event_loop
{

    quit_action run(SDL_Renderer * renderer, program_config const & cfg);

    private:

    void handle_other_event(SDL_Event const & e, widget_context & ctx, std::shared_ptr<list_view> lv);

    navigation_event_sender _nes;
};
