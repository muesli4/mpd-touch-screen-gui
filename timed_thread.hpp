#ifndef TIMED_THREAD_HPP
#define TIMED_THREAD_HPP

#include <chrono>
#include <thread>
#include <functional>
#include <condition_variable>


template <typename Duration>
struct timed_thread
{
    typedef Duration duration_type;
    typedef std::function<void(void)> task_type;

    timed_thread(task_type && t, duration_type d)
        : _task(t)
        , _duration(d)
        , _thread()
        , _cv_canceled()
    {
    }

    void start()
    {
        _thread = std::thread([&](){
            auto res = _cv_canceled.wait_for(_duration);

            if (res == std::cv_status::timeout)
            {
                _task();
            }
        });
    }

    void cancel()
    {
        _cv_canceled.notify_one();
        _thread.join();
    }

    private:
    
    task_type _task;
    duration_type _duration;
    std::thread _thread;
    std::condition_variable _cv_canceled;
};

#endif

