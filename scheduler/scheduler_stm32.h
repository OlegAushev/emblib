#pragma once


#if defined(EMBLIB_ARM)


#include "../core.h"
#include "../static_vector.h"
#include <chrono>


namespace emb {


namespace scheduler {


enum class task_execsts {
    success,
    fail
};


class basic_scheduler {
private:
    static inline std::chrono::milliseconds (*_now)() = [](){ return std::chrono::milliseconds(0); };

    static constexpr size_t max_taskcount{8};

    struct task {
        std::chrono::milliseconds period;
        std::chrono::milliseconds timepoint;
        task_execsts (*func)(size_t);
    };
    static inline emb::static_vector<task, max_taskcount> _tasks;

    static task_execsts empty_task(size_t) { return task_execsts::success; }

private:
    static inline std::chrono::milliseconds _delayed_task_start{0};
    static inline std::chrono::milliseconds _delayed_task_delay{0};
    static void empty_delayed_task() {}
    static inline void (*_delayed_task)() = empty_delayed_task;

public:
    basic_scheduler() = delete;
    static void init(std::chrono::milliseconds (*get_now_func)()) {
        _now = get_now_func;
    }

    static void add_task(task_execsts (*func)(size_t), std::chrono::milliseconds period) {
        task task_ = {period, _now(), func};
        _tasks.push_back(task_);
    }

    static void set_task_period(size_t index, std::chrono::milliseconds period) {
        if (index < _tasks.size()) {
            _tasks[index].period = period;
        }
    }

    static void add_delayed_task(void (*task)(), std::chrono::milliseconds delay) {
        _delayed_task = task;
        _delayed_task_delay = delay;
        _delayed_task_start = _now();
    }

    static void run() {
        auto now = _now();

        for (size_t i = 0; i < _tasks.size(); ++i) {
            if (now >= (_tasks[i].timepoint + _tasks[i].period)) {
                if (_tasks[i].func(i) == task_execsts::success) {
                    _tasks[i].timepoint = now;
                }
            }
        }

        if (_delayed_task_delay.count() != 0) {
            if (now >= (_delayed_task_start + _delayed_task_delay)) {
                _delayed_task();
                _delayed_task_delay = std::chrono::milliseconds(0);
            }
        }
    }

    static void reset() {
        for (size_t i = 0; i < _tasks.size(); ++i) {
            _tasks[i].timepoint = _now();
        }
    }
};


} // namespace scheduler


} // namespace emb


#endif
