// SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef USER_EVENT_HPP
#define USER_EVENT_HPP

#include <cstdint>

#include <libwtk-sdl2/navigation_type.hpp>
#include <SDL2/SDL_events.h>

// As SDL_PushEvent itself is thread safe, this class is too.
class generic_user_event_sender
{

    void push_int(int code) const;
    void push_int_with_payload(int code, int data1) const;

    uint32_t const _user_event_type;

    public:

    generic_user_event_sender();
    generic_user_event_sender(uint32_t user_event_type) noexcept;
    generic_user_event_sender(generic_user_event_sender const & other);

    template <typename T>
    void push(T t) const;

    template <typename T, typename P>
    void push_with_payload(T t, P p) const;

    template <typename T>
    void read(SDL_Event const & e, T & t) const;

    template <typename T, typename P>
    void read_with_payload(SDL_Event const & e, T & t, P & p) const;

    bool is_event_type(uint32_t event_type) const;
};


template <typename T>
void generic_user_event_sender::push(T t) const
{
    push_int(static_cast<int>(t));
}

template <typename T, typename P>
void generic_user_event_sender::push_with_payload(T t, P p) const
{
    push_int_with_payload(static_cast<int>(t), static_cast<int>(p));
}

template <typename T>
void generic_user_event_sender::read(SDL_Event const & e, T & t) const
{
    t = static_cast<T>(e.user.code);
}

template <typename T, typename P>
void generic_user_event_sender::read_with_payload(SDL_Event const & e, T & t, P & p) const
{
    t = static_cast<T>(e.user.code);

    std::uintptr_t const tmp = reinterpret_cast<std::uintptr_t>(e.user.data1);
    p = static_cast<P>(tmp);
}

struct simple_event_sender
{
    bool is_event_type(uint32_t event_type) const;

    void push() const;

    private:

    generic_user_event_sender _gues;
};

template <typename T>
class enum_user_event_sender
{
    generic_user_event_sender _gues;

    public:

    void push(T t) const
    {
        _gues.push(t);
    }

    T read(SDL_Event const & e) const
    {
        T tmp;
        _gues.read(e, tmp);
        return tmp;
    }

    bool is_event_type(uint32_t event_type) const
    {
        return _gues.is_event_type(event_type);
    }

};

#endif
