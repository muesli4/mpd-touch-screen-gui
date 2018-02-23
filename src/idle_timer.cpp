#include <SDL2/SDL_stdinc.h>
#include "idle_timer.hpp"
#include "user_event.hpp"

idle_timer_info::idle_timer_info(enum_user_event_sender<idle_timer_event_type> & ies)
    : _ies(ies)
{
}

void idle_timer_info::sync()
{
    _start_tp = std::chrono::steady_clock::now();
    _last_activity_tp = _start_tp;
}

void idle_timer_info::signal_user_activity()
{
    _last_activity_tp = std::chrono::steady_clock::now();
}

std::chrono::milliseconds idle_timer_info::remaining_ms()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(_last_activity_tp - _start_tp);
}

#include <iostream>
std::chrono::milliseconds idle_timer_info::callback()
{
    auto rem_ms = remaining_ms();

    if (rem_ms.count() > 0)
    {
        sync();
        return rem_ms;
    }
    else
    {
        _ies.push(idle_timer_event_type::IDLE_TIMER_EXPIRED);
        return std::chrono::milliseconds(0);
    }
}

Uint32 idle_timer_cb(Uint32 interval, void * iti_ptr)
{
    return
        reinterpret_cast<idle_timer_info *>(iti_ptr)
            ->callback()
            .count();
}

