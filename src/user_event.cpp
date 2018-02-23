#include <stdexcept>

#include <SDL2/SDL_events.h>

#include "user_event.hpp"

generic_user_event_sender::generic_user_event_sender(uint32_t user_event_type) noexcept
    : _user_event_type(user_event_type)
{
}

generic_user_event_sender::generic_user_event_sender()
    : generic_user_event_sender(SDL_RegisterEvents(1))
{
    if (_user_event_type == static_cast<uint32_t>(-1))
    {
        throw std::runtime_error("out of SDL user events");
    }
}

generic_user_event_sender::generic_user_event_sender(generic_user_event_sender const & other)
    : generic_user_event_sender(other._user_event_type)
{
}

bool generic_user_event_sender::is_event_type(uint32_t event_type) const
{
    return event_type == _user_event_type;
}

void generic_user_event_sender::push_int(int code) const
{
    push_int_with_payload(code, 0);
}

void generic_user_event_sender::push_int_with_payload(int code, int data1) const
{
    SDL_Event e;
    SDL_memset(&e, 0, sizeof(e));

    e.type = _user_event_type;
    e.user.code = static_cast<int>(code);
    std::uintptr_t const p = static_cast<std::uintptr_t>(data1);
    e.user.data1 = reinterpret_cast<void *>(p);
    if (SDL_PushEvent(&e) < 0)
    {
        throw std::runtime_error(std::string("failed to push event: ") + SDL_GetError());
    }
}

