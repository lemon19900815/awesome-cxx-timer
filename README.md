# cxx-timer

Awesome cpp timer: A C++11 cross-platform single-header timer library.

**features:**

1. thread-safe
2. high-precision(average delay within 1ms)
3. easy to use(just include the `cxx-timer.h`)



## 1. design

1. On Windows, after `std::this_thread::sleep_for` some milliseconds, the thread is resumed, but the time is passed more than the sleep time. I use the `condition_variable` to simulate it.
2. the `time_event_t` callback is run on another thread to avoiding prolonged execution of callbacks that impact timer accuracy.



## 2. timer interface

```c++
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
```



## 3. usage

### 3.1 create once timer

```c++
using namespace utility::timer;

auto msec = 10;
timer_iface::get().create_timer(msec, []() {
    std::cout << "timer fired." << std::endl;
});
```



### 3.2 create repeat timer

```c++
using namespace utility::timer;

auto msec = 10;
auto repeat = 100;
timer_iface::get().create_repeat_timer(msec, repeat, []() {
    std::cout << "timer fired." << std::endl;
});
```



### 3.3 cancel timer

```c++
using namespace utility::timer;

auto msec = 10;
auto repeat = 100;
auto id = timer_iface::get().create_repeat_timer(msec, repeat, []() {
    std::cout << "timer fired." << std::endl;
});

std::this_thread::sleep_for(std::chrono::milliseconds(10));
timer_iface::get().cancel_timer(id);
```



## 4. unit-test

use `doctest.h` to do unit test.



