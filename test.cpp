#include <iostream>
#include <functional>

#include "cxx-timer.h"
using namespace utility::timer;

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

TEST_CASE("test timer_iface::create_timer")
{
    auto msec = 10;
    std::atomic_bool timer_fired { false };
    auto id = timer_iface::get().create_timer(msec, [&timer_fired]()
    {
        timer_fired.store(true);
    });

    // after a moment, the timer must be fired.
    std::this_thread::sleep_for(std::chrono::milliseconds(msec + 1));
    CHECK_EQ(timer_fired.load(), true);
}

TEST_CASE("test timer_iface::create_repeat_timer")
{
    auto msec = 10;
    auto repeat = 10;

    std::atomic_int fired_count { 0 };
    auto id = timer_iface::get().create_repeat_timer(msec, repeat, [&fired_count]()
    {
        fired_count.store(fired_count.load() + 1);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(msec * (repeat+1)));

    // after a moment, the fired count equal repeat.
    CHECK_EQ(fired_count, repeat);
}

TEST_CASE("test timer_iface::cancel_timer")
{
    auto msec = 10;
    auto repeat = 10;

    std::atomic_int fired_count { 0 };
    auto id = timer_iface::get().create_repeat_timer(msec, repeat, [&fired_count]()
    {
        // nothing to do.
        fired_count.store(fired_count.load() + 1);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(msec * repeat / 2));

    // after a moment, the fired count equal repeat.
    CHECK_EQ(timer_iface::get().cancel_timer(id), true);
    CHECK_LE(fired_count, repeat);
}

TEST_CASE("test timer accuacy")
{
    auto t0 = detail::tick_count();

    auto interval = 1;
    auto repeat = 1000;
    int64_t max_delta = 0;

    // long time run although cause timer accuacy error.
    // it's caused by system thread schedule.
    timer_iface::get().create_repeat_timer(interval, repeat,
                                          [&t0, &max_delta, interval]()
    {
        auto t1 = detail::tick_count();
        auto delta = std::abs(t1 - t0 - interval);
        max_delta = std::max<int64_t>(delta, max_delta);
        CHECK(delta <= 5);
        t0 = t1;
    });

    auto wait_time = repeat * (interval + 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));

    std::cout << "max_delta = " << max_delta << std::endl;
}

