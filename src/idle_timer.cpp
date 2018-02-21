#include <SDL2/SDL_stdinc.h>
#include "idle_timer.hpp"
#include "user_event.hpp"

idle_timer_info::idle_timer_info(user_event_sender ues)
    : ues(ues)
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

Uint32 idle_timer_cb(Uint32 interval, void * iti_ptr)
{

    idle_timer_info & iti = *reinterpret_cast<idle_timer_info *>(iti_ptr);

    auto rem_ms = iti.remaining_ms();

    if (rem_ms.count() > 0)
    {
        iti.sync();
        return rem_ms.count();
    }
    else
    {
        iti.ues.push(user_event::TIMER_EXPIRED);
        return 0;
    }
}

