#ifndef MPD_CONTROL_HPP
#define MPD_CONTROL_HPP

#include <functional>
#include <mutex>
#include <queue>
#include <future>

#include <mpd/client.h>

struct mpd_control
{
    mpd_control(std::function<void(std::string)> new_song_path_cb, std::function<void(bool)> random_cb);
    ~mpd_control();

    void run();

    void stop();
    void toggle_pause();
    void inc_volume(unsigned int amount);
    void dec_volume(unsigned int amount);
    void next_song();
    void prev_song();

    void set_random(bool value);

    std::future<bool> get_random();

    std::future<std::string> get_current_title();
    std::future<std::string> get_current_artist();
    std::future<std::string> get_current_album();

    private:

    std::future<std::string> get_current_tag(enum mpd_tag_type type);

    void add_external_task(std::function<void(mpd_connection *)> t);

    void new_song_cb(mpd_song * s);

    mpd_connection * _c;

    bool _run;

    std::function<void(std::string)> _new_song_path_cb;
    std::function<void(bool)> _random_cb;

    std::mutex _external_tasks_mutex;
    std::deque<std::function<void(mpd_connection *)>> _external_tasks;

    std::mutex _external_song_queries_mutex;
    std::deque<std::function<void(mpd_connection *, mpd_song *)>> _external_song_queries;
};

#endif

