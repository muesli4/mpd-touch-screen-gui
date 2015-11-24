#ifndef MPD_CONTROL_HPP
#define MPD_CONTROL_HPP

#include <functional>
#include <mutex>
#include <queue>

#include <mpd/client.h>

struct mpd_control
{
    mpd_control(std::function<void(std::string)> new_song_path_cb);
    ~mpd_control();

    void run();

    void stop();
    void toggle_pause();
    void inc_volume(unsigned int amount);
    void dec_volume(unsigned int amount);
    void next_song();
    void prev_song();

    private:

    void add_external_task(std::function<void(mpd_connection *)> t);

    void new_song_cb(mpd_song * s);

    mpd_connection * _c;

    bool _run;

    std::function<void(std::string)> _new_song_path_cb;

    std::mutex _external_tasks_mutex;
    std::deque<std::function<void(mpd_connection *)>> _external_tasks;
};

#endif

