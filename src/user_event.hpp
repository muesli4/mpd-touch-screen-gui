#ifndef USER_EVENT_HPP
#define USER_EVENT_HPP

#include <cstdint>
#include <libwtk-sdl2/navigation_type.hpp>

// TODO change with introduction of sum types
enum class user_event
{
    RANDOM_CHANGED,
    SONG_CHANGED,
    PLAYLIST_CHANGED,
    TIMER_EXPIRED,
    REFRESH,
    NAVIGATION,
    ACTIVATE
};

struct user_event_sender
{
    user_event_sender();
    user_event_sender(uint32_t user_event_type) noexcept;

    // TODO allows pushing invalid events
    void push(user_event ue);
    void push_navigation(navigation_type nt);

    bool is(uint32_t uet);

    private:
    void push(user_event ue, int data1);

    uint32_t const _user_event_type;
};

void push_user_event(uint32_t event_type, user_event ue, int data1 = 0);
void push_navigate_event(uint32_t event_type, navigation_type nt);
void push_activate_event(uint32_t event_type);

#endif
