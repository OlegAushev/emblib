#pragma once

#if __cplusplus >=201700

#include <emblib/core.hpp>
#include <emblib/chrono.hpp>
#include <emblib/static_vector.hpp>


namespace emb {
namespace scheduler {

enum class task_execstatus {
    success,
    fail
};

class basic_scheduler {
private:
    static inline bool _initialized{false};
    static constexpr size_t max_taskcount{8};

    struct task {
        std::chrono::milliseconds period;
        std::chrono::milliseconds timepoint;
        task_execstatus (*func)(size_t);
    };
    static inline emb::static_vector<task, max_taskcount> _tasks;

    static task_execstatus empty_task(size_t) { return task_execstatus::success; }

private:
    static inline std::chrono::milliseconds _delayed_task_start{0};
    static inline std::chrono::milliseconds _delayed_task_delay{0};
    static void empty_delayed_task() {}
    static inline void (*_delayed_task)() = empty_delayed_task;

public:
    basic_scheduler() = delete;
    static void init() {
        assert(emb::chrono::steady_clock::initialized());
        _initialized = true;
    }

    static void add_task(task_execstatus (*func)(size_t), std::chrono::milliseconds period) {
        task task_ = {period, emb::chrono::steady_clock::now(), func};
        _tasks.push_back(task_);
    }

    static void set_task_period(size_t index, std::chrono::milliseconds period) {
        if (index < _tasks.size()) {
            _tasks[index].period = period;
        }
    }

    static void add_delayed_task(void (*func)(), std::chrono::milliseconds delay) {
        _delayed_task = func;
        _delayed_task_delay = delay;
        _delayed_task_start = emb::chrono::steady_clock::now();
    }

    static void run() {
        auto now = emb::chrono::steady_clock::now();

        for (size_t i = 0; i < _tasks.size(); ++i) {
            if (now >= (_tasks[i].timepoint + _tasks[i].period)) {
                if (_tasks[i].func(i) == task_execstatus::success) {
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
            _tasks[i].timepoint = emb::chrono::steady_clock::now();
        }
    }
};

} // namespace scheduler
} // namespace emb

#endif
