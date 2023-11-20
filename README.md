# cxx-timer

Awesome cpp timer: A C++11 cross-platform single-header timer library.

**Features:**

1. thread-safe
2. high-precision(average delay within 1ms)
3. easy to use(just include the `cxx-timer.h`)



## 1. Design

1. On Windows and Linux, after `std::this_thread::sleep_for` some milliseconds, the thread is resumed, but the passed period is more than sleep time. Using `condition_variable` replace `sleep_for`.
2. The `time_event_t` callback is run on another thread to avoiding slow execution of callbacks that impact timer accuracy.



## 2. Interface

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

// get interface implement.
static timer_iface& get_timer_iface();
```



## 3. Usage

### 3.1 create once timer

```c++
using namespace utility::timer;

auto msec = 10;
get_timer_iface().create_timer(msec, []() {
    std::cout << "timer fired." << std::endl;
});
```



### 3.2 create repeat timer

```c++
using namespace utility::timer;

auto msec = 10;
auto repeat = 100;
get_timer_iface().create_repeat_timer(msec, repeat, []() {
    std::cout << "timer fired." << std::endl;
});
```



### 3.3 cancel timer

```c++
using namespace utility::timer;

auto msec = 10;
auto repeat = 100;
auto id = get_timer_iface().create_repeat_timer(msec, repeat, []() {
    std::cout << "timer fired." << std::endl;
});

std::this_thread::sleep_for(std::chrono::milliseconds(10));
timer_iface::get().cancel_timer(id);
```



## 4. Test

use `doctest.h` to do test, please see test.cpp.



## 5. Reference

1. https://zhuanlan.zhihu.com/p/400200921?utm_id=0



## 6. Future

- [ ] modify thread priority.
- [ ] modify thread affinity.

