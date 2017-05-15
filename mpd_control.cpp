#include <chrono>
#include <thread>
#include <memory>

#include "mpd_control.hpp"
#include "util.hpp"


mpd_control::mpd_control(std::function<void(std::string, unsigned int)> new_song_cb, std::function<void(bool)> random_cb)
    : _c(mpd_connection_new(0, 0, 0))
    , _run(true)
    , _new_song_cb(new_song_cb)
    , _random_cb(random_cb)
{
}

mpd_control::~mpd_control()
{
    mpd_connection_free(_c);
}

void mpd_control::run()
{
    mpd_song * last_song = mpd_run_current_song(_c);

    if (last_song != 0) new_song_cb(last_song);

    while (_run)
    {
        mpd_send_idle_mask(_c, static_cast<mpd_idle>(MPD_IDLE_PLAYER | MPD_IDLE_OPTIONS));

        // TODO use condition variable here!
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        enum mpd_idle idle_event = mpd_run_noidle(_c);
        if (idle_event & MPD_IDLE_PLAYER)
        {
            mpd_song * song = mpd_run_current_song(_c);

            if (song != 0)
            {
                if (last_song != 0)
                {
                    if (mpd_song_get_id(song) != mpd_song_get_id(last_song))
                    {
                        new_song_cb(song);
                    }
                    mpd_song_free(last_song);
                }
            }
            last_song = song;
        }
        if (idle_event & MPD_IDLE_OPTIONS)
        {
            mpd_status * s = mpd_run_status(_c);
            _random_cb(mpd_status_get_random(s));
            mpd_status_free(s);
        }
        {
            scoped_lock lock(_external_tasks_mutex);

            while (!_external_tasks.empty())
            {
                _external_tasks.front()(_c);
                _external_tasks.pop_front();
            }
        }
        {
            scoped_lock lock(_external_song_queries_mutex);

            while (!_external_song_queries.empty())
            {
                _external_song_queries.front()(_c, last_song);
                _external_song_queries.pop_front();
            }
        }
    }
    mpd_song_free(last_song);
}

void mpd_control::stop()
{
    _run = false;
}

void mpd_control::toggle_pause()
{
    add_external_task([](mpd_connection * c){ mpd_run_toggle_pause(c); });
}

void mpd_control::inc_volume(unsigned int amount)
{
    add_external_task([amount](mpd_connection * c)
    {
        mpd_status * s = mpd_run_status(c);
        int new_volume = std::min(100, mpd_status_get_volume(s) + static_cast<int>(amount));
        mpd_run_set_volume(c, new_volume);
    });
}

void mpd_control::dec_volume(unsigned int amount)
{
    add_external_task([amount](mpd_connection * c)
    {
        mpd_status * s = mpd_run_status(c);
        int new_volume = std::max(0, mpd_status_get_volume(s) - static_cast<int>(amount));
        mpd_run_set_volume(c, new_volume);
    });
}

void mpd_control::next_song()
{
    add_external_task([](mpd_connection * c){ mpd_run_next(c); });
}

void mpd_control::prev_song()
{
    add_external_task([](mpd_connection * c){ mpd_run_previous(c); });
}

void mpd_control::play_position(int pos)
{
    add_external_task([pos](mpd_connection * c){ mpd_run_play_pos(c, pos); });
}

void mpd_control::set_random(bool value)
{
    add_external_task([value](mpd_connection * c) { mpd_run_random(c, value); });
}

bool mpd_control::get_random()
{
    typedef std::promise<bool> promise_type;
    // TODO make_unique
    std::shared_ptr<promise_type> promise_ptr = std::make_shared<promise_type>();

    {
        scoped_lock lock(_external_tasks_mutex);
        _external_tasks.push_back([promise_ptr](mpd_connection * c)
        {
            mpd_status * s = mpd_run_status(c);
            promise_ptr->set_value(mpd_status_get_random(s));
            mpd_status_free(s);
        });
    }

    return promise_ptr->get_future().get();
}

std::string mpd_control::get_current_title()
{
    return get_current_tag(MPD_TAG_TITLE);
}

std::string mpd_control::get_current_artist()
{
    return get_current_tag(MPD_TAG_ARTIST);
}

std::string mpd_control::get_current_album()
{
    return get_current_tag(MPD_TAG_ALBUM);
}

std::vector<std::string> mpd_control::get_current_playlist()
{
    typedef std::promise<std::vector<std::string>> promise_type;
    // TODO make_unique
    std::shared_ptr<promise_type> promise_ptr = std::make_shared<promise_type>();

    {
        scoped_lock lock(_external_tasks_mutex);
        _external_tasks.push_back([promise_ptr](mpd_connection * c)
        {
            mpd_status * s = mpd_run_status(c);
            std::vector<std::string> playlist;
            playlist.reserve(mpd_status_get_queue_length(s));
            mpd_status_get_queue_version(s);

            mpd_send_list_queue_meta(c);

            mpd_song * song;
            while ((song = mpd_recv_song(c)) != nullptr)
            {
                playlist.push_back(
                    string_from_ptr(mpd_song_get_tag(song, MPD_TAG_ARTIST, 0))
                    + " - "
                    + string_from_ptr(mpd_song_get_tag(song, MPD_TAG_TITLE, 0))
                );
                mpd_song_free(song);
            }

            promise_ptr->set_value(playlist);
            mpd_status_free(s);
        });
    }

    return promise_ptr->get_future().get();
}


std::string mpd_control::get_current_tag(enum mpd_tag_type type)
{
    typedef std::promise<std::string> promise_type;
    // TODO make_unique
    std::shared_ptr<promise_type> promise_ptr = std::make_shared<promise_type>();

    {
        scoped_lock lock(_external_song_queries_mutex);
        _external_song_queries.push_back([type, promise_ptr](mpd_connection * c, mpd_song * s)
        {
            char const * const cstr = mpd_song_get_tag(s, type, 0);
            promise_ptr->set_value(string_from_ptr(cstr));
        }
        );
    }

    return promise_ptr->get_future().get();
}


void mpd_control::add_external_task(std::function<void(mpd_connection *)> t)
{
    scoped_lock lock(_external_tasks_mutex);
    _external_tasks.push_back(t);
}

void mpd_control::new_song_cb(mpd_song * s)
{
    _new_song_cb(mpd_song_get_uri(s), mpd_song_get_pos(s));
}
