#ifndef IDLE_TIMER_HPP
#define IDLE_TIMER_HPP

#include <chrono>

#include "user_event.hpp"

enum class idle_timer_event_type
{
    IDLE_TIMER_EXPIRED
};

// This is a helper class to use a single SDL timer without the need to stop it
// all the time.
struct idle_timer_info
{
    idle_timer_info(enum_user_event_sender<idle_timer_event_type> & ies);

    void sync();

    void signal_user_activity();

    std::chrono::milliseconds remaining_ms();

    std::chrono::milliseconds callback();

    private:

    enum_user_event_sender<idle_timer_event_type> & _ies;

    std::chrono::time_point<std::chrono::steady_clock> _last_activity_tp;
    std::chrono::time_point<std::chrono::steady_clock> _start_tp;
};

Uint32 idle_timer_cb(Uint32 interval, void * iti_ptr);

#endif

