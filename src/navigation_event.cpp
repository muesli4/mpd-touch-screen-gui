#include "navigation_event.hpp"

void navigation_event_sender::push(navigation_event ne) const
{
    _gues.push_with_payload(ne.type, ne.nt);
}

void navigation_event_sender::read(SDL_Event const & e, navigation_event & ne) const
{
    _gues.read_with_payload(e, ne.type, ne.nt);
}

bool navigation_event_sender::is_event_type(uint32_t event_type) const
{
    return _gues.is_event_type(event_type);
}
