#include <stdexcept>

#include <SDL2/SDL_events.h>

#include "user_event.hpp"

user_event_sender::user_event_sender(uint32_t user_event_type) noexcept
    : _user_event_type(user_event_type)
{
}

user_event_sender::user_event_sender()
    : user_event_sender(SDL_RegisterEvents(1))
{
    if (_user_event_type == static_cast<uint32_t>(-1))
    {
        throw std::runtime_error("out of SDL user events");
    }
}

void user_event_sender::push(user_event ue)
{
    push(ue, 0);
}

void user_event_sender::push_navigation(navigation_type nt)
{
    push(user_event::NAVIGATION, static_cast<int>(nt));
}

bool user_event_sender::is(uint32_t uet)
{
    return uet == _user_event_type;
}

void user_event_sender::push(user_event ue, int data)
{
    SDL_Event e;
    SDL_memset(&e, 0, sizeof(e));
    e.type = _user_event_type;
    e.user.code = static_cast<int>(ue);
    e.user.data1 = reinterpret_cast<void *>(data);
    if (SDL_PushEvent(&e) < 0)
    {
        throw std::runtime_error(std::string("failed to push event: ") + SDL_GetError());
    }
}

