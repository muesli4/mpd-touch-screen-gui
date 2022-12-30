// SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef MPD_CONTROL_HPP
#define MPD_CONTROL_HPP

#include <functional>
#include <mutex>
#include <queue>
#include <future>
#include <optional>

#include <mpd/client.h>

#include "dynamic_image_data.hpp"

#if defined(HAVE_POLL_H) && defined(HAVE_SYS_EVENTFD_H) && defined(HAVE_UNISTD_H)
#pragma message ( "Compiling with eventfd polling support." )
#define USE_POLL
#endif

template <typename F>
class callback_deque
{
    typedef std::deque<std::function<F>> deque_type;
    std::mutex _mutex;

    deque_type _active_deque;
    deque_type _buffer_deque;

    public:

    void add(std::function<F> && f)
    {
        std::scoped_lock lock(_mutex);
        _active_deque.push_back(f);
    }

    template <typename... Args>
    void run(Args... args)
    {
        // Keep locking time to a minimum by swapping out queues and because of
        // that callbacks can add new callbacks that are not executed in the
        // same batch.
        {
            std::scoped_lock lock(_mutex);
            if (_active_deque.empty())
            {
                return;
            }
            else
            {
                _active_deque.swap(_buffer_deque);
            }
        }

        do
        {
            _buffer_deque.front()(args...);
            _buffer_deque.pop_front();
        }
        while (!_buffer_deque.empty());
    }
};

struct playlist_change_info
{
    typedef std::vector<std::pair<unsigned int, std::string>> diff_type;

    playlist_change_info(int nv, diff_type && cp, unsigned int l);

    unsigned int new_version;
    diff_type changed_positions;
    unsigned int new_length;
};

struct song_location
{
    std::string path;
    unsigned int pos;
};

struct status_info
{
};

struct mpd_control
{
    mpd_control
        ( std::function<void(std::optional<song_location>)> new_song_cb
        , std::function<void(bool)> random_cb
        , std::function<void()> playlist_changed_cb
        , std::function<void(mpd_state)> playback_state_changed_cb
        );
    ~mpd_control();

    void run();

    void stop();
    void toggle_pause();
    void inc_volume(unsigned int amount);
    void dec_volume(unsigned int amount);
    void next_song();
    void prev_song();

    void play_position(int pos);

    void set_random(bool value);
    bool get_random();
    void toggle_random();

    mpd_state get_state();

    std::string get_current_title();
    std::string get_current_artist();
    std::string get_current_album();

    std::pair<std::vector<std::string>, unsigned int> get_current_playlist();

    playlist_change_info get_current_playlist_changes(unsigned int version);

    std::optional<dynamic_image_data> get_albumart(std::string path);
    std::optional<dynamic_image_data> get_readpicture(std::string path);

    private:

    // wait for next event
    void wait();

    // wake up event loop to handle local events
    void notify();

    std::optional<dynamic_image_data> get_cover(std::string path,
                                                bool (* send_fun)(mpd_connection *, char const *, unsigned),
                                                int (* recv_fun)(mpd_connection *, void *, size_t),
                                                int (* run_fun)(mpd_connection *, char const *, unsigned, void *, size_t));
    void read_cover_chunk(mpd_connection * c, std::promise<std::optional<dynamic_image_data>> & result, byte_buffer & buffer, size_t & current_offset, std::string const & path,
                          int (* run_fun)(mpd_connection *, char const *, unsigned, void *, size_t));
    void handle_read_cover_chunk_result(std::promise<std::optional<dynamic_image_data>> & result, byte_buffer & buffer, size_t & current_offset, std::string const & path,
                                        int (* run_fun)(mpd_connection *, char const *, unsigned, void *, size_t), int read_bytes);

    std::string get_current_tag(enum mpd_tag_type type);

    void add_external_task(std::function<void(mpd_connection *)> && t);

    template <typename R>
    R add_external_task_with_return(std::function<R(mpd_connection *)> && f)
    {
        std::promise<R> promise;
        add_external_task([&promise, f](mpd_connection * c)
        {
            promise.set_value(f(c));
        });
        return promise.get_future().get();
    }

    void new_song_cb(mpd_song * s);

    mpd_connection * _c;

    bool _run;

    std::function<void(std::optional<song_location>)> _new_song_cb;
    std::function<void(bool)> _random_cb;
    std::function<void()> _playlist_changed_cb;
    std::function<void(mpd_state)> _playback_state_changed_cb;

    callback_deque<void(mpd_connection *)> _external_tasks;
    callback_deque<void(mpd_connection *, mpd_song *)> _external_song_queries;

#ifdef USE_POLL
    // a file descriptor for thread communication
    int _eventfd;
#endif
};

#endif

