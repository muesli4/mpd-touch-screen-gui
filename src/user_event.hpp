#ifndef USER_EVENT_HPP
#define USER_EVENT_HPP

#include <cstdint>

enum class user_event
{
    RANDOM_CHANGED,
    SONG_CHANGED,
    PLAYLIST_CHANGED,
    TIMER_EXPIRED,
    REFRESH
};

void push_user_event(uint32_t event_type, user_event ue);

#endif
