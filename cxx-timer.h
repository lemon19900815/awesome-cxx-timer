/*

Copyright (c) 2023-2033 lemon19900815@buerjia

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/

#ifndef __UTILITY_TIMER_H__
#define __UTILITY_TIMER_H__

#include <functional>
#include <thread>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <map>
#include <list>
#include <unordered_map>
#include <atomic>
#include <cassert>
#include <memory>
#include <condition_variable>

namespace utility
{
namespace timer
{

// the timer callback function signature.
using timer_event_t = std::function<void()>;

// timer interface defination.
class timer_iface
{
public:
    virtual ~timer_iface() = default;

    // create a once timer delay msec.
    virtual int32_t create_timer(int32_t msec, timer_event_t cb) = 0;
    // create a repeaet timer delay msec.
    virtual int32_t create_repeat_timer(int32_t msec, int32_t repeat,
                                        timer_event_t cb) = 0;
    // cancel a timer with id.
    virtual bool cancel_timer(int32_t timer_id) = 0;

    // get interface implement.
    static timer_iface& get();
};

// timer_iface implement.
namespace impl
{

struct timer_t
{
    int32_t        msec{ 0 };
    int64_t        expires{ 0 };
    int32_t        timer_id{ 0 };
    int32_t        repeat{ 0 };
    timer_event_t  timer_cb{ nullptr };

    using ptr = std::shared_ptr<struct timer_t>;
    using wptr = std::weak_ptr<struct timer_t>;
};

// the timer(weak) bucket expired on same time.
using timer_bucket = std::list<timer_t::wptr>;
// the expired timer buckets managed by key：expired time
using expired_timer_buckets = std::map<int64_t, timer_bucket>;
// the timer(strong) object set managed by key：timer id
using id_timers = std::unordered_map<int32_t, timer_t::ptr>;

// steady clock tick count(milliseconds) on startup.
static int64_t tick_count()
{
    using namespace std::chrono;
    auto now_ms = duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count();
    return now_ms;
}

class timer_mgr : public timer_iface
{
public:
    int32_t create_timer(int32_t msec, timer_event_t cb) override;
    int32_t create_repeat_timer(int32_t msec, int32_t repeat,
                                timer_event_t cb) override;

    bool cancel_timer(int32_t timer_id) override;

public:
    timer_mgr(const timer_mgr&) = delete;
    timer_mgr& operator=(const timer_mgr&) = delete;

    timer_mgr() noexcept
    {
        stop_.store(false);
        schedule_thd_ = std::thread(&timer_mgr::schedule, this);
        event_thd_ = std::thread(&timer_mgr::run_timer_event, this);
    }

    virtual ~timer_mgr() noexcept
    {
        stop_.store(true);
        schedule_thd_.join();
        event_cv_.notify_one();
        event_thd_.join();
    }

private:
    int32_t setup_timer(int32_t msec, int32_t repeat, timer_event_t cb);
    void setup_timer(timer_t::ptr& timer);

    // timer schedule task thread.
    void schedule();

    // run timer callback event in another thread.
    void run_timer_event();

    int32_t alloc_timerid();

    // calc timer expired time.
    int64_t calc_expired_time(int32_t msec);

    void get_expired_timers(expired_timer_buckets& timers);
    void process_expired_timers(expired_timer_buckets& timers);

    int64_t get_min_expired_time();

    // sleep for a while.
    void sleep_ms(int32_t msec);

private:
    std::atomic_bool stop_{ true };

    // timer schedule thread.
    std::mutex schedule_mtx_;
    std::thread schedule_thd_;

    // key: expired time.
    expired_timer_buckets bucket_timers_;
    // key: timer id.
    id_timers id_timers_;

    // timer id alloced start number.
    std::atomic_int id_{ 0 };

    std::mutex event_mtx_;
    std::thread event_thd_;
    std::condition_variable event_cv_;
    std::list<timer_event_t> timer_events_;

    // use condition_variable to simulate sleep_for
    // because the sleep_for is not reliable on windows.
    std::mutex sleep_mtx_;
    std::condition_variable sleep_cv_;
};

inline int64_t timer_mgr::calc_expired_time(int32_t msec)
{
    // timeout = (msec + now_time)/accuracy_ * accuracy_;
    //return (now_msec_time() + msec) / accuracy_ * accuracy_;
    return tick_count() + msec;
}

inline int32_t timer_mgr::create_timer(int32_t msec, timer_event_t cb)
{
    return setup_timer(msec, 1, std::move(cb));
}

inline int32_t timer_mgr::create_repeat_timer(int32_t msec, int32_t repeat,
                                              timer_event_t cb)
{
    assert(repeat > 0);
    return setup_timer(msec, repeat, std::move(cb));
}

inline bool timer_mgr::cancel_timer(int32_t timer_id)
{
    bool result = false;

    {
        // just delete the shared_ptr timer_obj,
        // then the weak_ptr lock() will return nullptr.
        std::lock_guard<std::mutex> guard(schedule_mtx_);
        result = id_timers_.erase(timer_id) == 1U;
    }

    return result;
}

inline int32_t timer_mgr::setup_timer(int32_t msec, int32_t repeat,
                                      timer_event_t cb)
{
    assert(repeat > 0);

    auto timer_id = alloc_timerid();

    auto timer = std::make_shared<timer_t>();
    timer->timer_id = timer_id;
    timer->repeat = repeat;
    timer->msec = msec;
    timer->timer_cb = std::move(cb);
    timer->expires = calc_expired_time(msec);

    {
        std::lock_guard<std::mutex> guard(schedule_mtx_);
        setup_timer(timer);
    }

    return timer_id;
}

inline void timer_mgr::setup_timer(timer_t::ptr& timer)
{
    id_timers_[timer->timer_id] = timer;
    bucket_timers_[timer->expires].emplace_back(timer);
}

inline int32_t timer_mgr::alloc_timerid()
{
    auto timerid = id_.load() + 1;
    id_.store(timerid);

    return timerid;
}

inline void timer_mgr::schedule()
{
    while (!stop_.load())
    {
        if (tick_count() >= get_min_expired_time())
        {
            expired_timer_buckets timers;
            get_expired_timers(timers);

            process_expired_timers(timers);
        }

        sleep_ms(1);
    }
}

inline void timer_mgr::run_timer_event()
{
    while (!stop_.load())
    {
        decltype(timer_events_) timers;

        {
            std::unique_lock<std::mutex> lock(event_mtx_);
            event_cv_.wait(lock, [this]()
            {
                return stop_.load() || !timer_events_.empty();
            });

            if (stop_.load())
            {
                // timer is stoped, don't run timer callback.
                return;
            }

            timers = std::move(timer_events_);
        }

        for (auto& cb : timers)
        {
            cb();
        }
    }
}

inline void timer_mgr::get_expired_timers(expired_timer_buckets& timers)
{
    std::lock_guard<std::mutex> lock(schedule_mtx_);
    auto now = tick_count();

    auto it = bucket_timers_.begin();
    while (it != bucket_timers_.end())
    {
        if (it->first > now)
        {
            break;
        }

        timers[it->first] = std::move(it->second);
        it = bucket_timers_.erase(it);
    }
}

inline void timer_mgr::process_expired_timers(expired_timer_buckets& timers)
{
    if (timers.empty())
    {
        return;
    }

    auto now = tick_count();
    decltype(timer_events_) timers_cb;

    // pick timers callback.
    for (auto& it : timers)
    {
        for (auto& weak_timer : it.second)
        {
            auto timer = weak_timer.lock();
            if (timer)
            {
                do
                {
                    timers_cb.emplace_back(timer->timer_cb);
                    timer->repeat -= 1;
                    timer->expires += timer->msec;

                    // accumulate delta time.
                } while (timer->repeat > 0 && timer->expires <= now);
            }
        }
    }

    if(!timers_cb.empty())
    {
        // timer callback will run in another thread.
        // Avoiding prolonged execution of callbacks that affect timer accuracy.
        std::lock_guard<std::mutex> guard(event_mtx_);
        timer_events_.insert(timer_events_.end(),
                               timers_cb.begin(), timers_cb.end());

        event_cv_.notify_one();
    }

    {
        std::lock_guard<std::mutex> guard(schedule_mtx_);
        for (auto& it : timers)
        {
            for (auto& weak_timer : it.second)
            {
                auto timer = weak_timer.lock();
                if (!timer)
                {
                    continue; // timer is canceled by user.
                }

                if (timer->repeat <= 0)
                {
                    id_timers_.erase(timer->timer_id);
                    continue;
                }

                setup_timer(timer);
            }
        }
    }
}

inline int64_t timer_mgr::get_min_expired_time()
{
    std::lock_guard<std::mutex> guard(schedule_mtx_);
    if (bucket_timers_.empty())
    {
        return std::numeric_limits<int64_t>::max();
    }

    return bucket_timers_.begin()->first;
}

inline void timer_mgr::sleep_ms(int32_t msec)
{
    // During on Windows testing, it was found that there was
    // an error of approximately 15 milliseconds.
    // sleep_for is not reliable on Windows.
    //std::this_thread::sleep_for(std::chrono::milliseconds(msec));

    std::unique_lock<std::mutex> lock(sleep_mtx_);

    auto expired_time = std::chrono::steady_clock::now();
    expired_time += std::chrono::milliseconds(msec);

    sleep_cv_.wait_until(lock, expired_time);
}
} // namespace impl

// hide in end of this file.
timer_iface& timer_iface::get()
{
    static impl::timer_mgr mgr;
    return mgr;
}

} // namespace timer
} // namespace utility

#endif // __UTILITY_TIMER_H__
