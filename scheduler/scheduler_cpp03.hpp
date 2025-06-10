#pragma once

#if __cplusplus < 201100

#include <emblib/chrono.hpp>
#include <emblib/core.hpp>
#include <emblib/scopedenum.hpp>
#include <emblib/static_vector.hpp>

namespace emb {
namespace scheduler {

// clang-format off
SCOPED_ENUM_DECLARE_BEGIN(task_execstatus) {
  success,
  fail
} SCOPED_ENUM_DECLARE_END(task_execstatus)
// clang-format off

    class basic_scheduler {
private:
  static bool _initialized;
  static size_t const max_taskcount = 8;

  struct task {
    emb::chrono::milliseconds period;
    emb::chrono::milliseconds timepoint;
    task_execstatus (*func)(size_t);
  };

  static emb::static_vector<task, max_taskcount> _tasks;

  static task_execstatus empty_task(size_t) { return task_execstatus::success; }
private:
  static emb::chrono::milliseconds _delayed_task_start;
  static emb::chrono::milliseconds _delayed_task_delay;

  static void empty_delayed_task() {}

  static void (*_delayed_task)();
private:
  basic_scheduler();
  basic_scheduler(basic_scheduler const& other);
  basic_scheduler& operator=(basic_scheduler const& other);
public:
  static void init() {
    assert(emb::chrono::steady_clock::initialized());
    _initialized = true;
  }

  static bool initialized() { return _initialized; }

  static void add_task(task_execstatus (*func)(size_t),
                       emb::chrono::milliseconds period) {
    task task_ = {period, emb::chrono::steady_clock::now(), func};
    _tasks.push_back(task_);
  }

  static void set_task_period(size_t index, emb::chrono::milliseconds period) {
    if (index < _tasks.size()) {
      _tasks[index].period = period;
    }
  }

  static void add_delayed_task(void (*func)(),
                               emb::chrono::milliseconds delay) {
    _delayed_task = func;
    _delayed_task_delay = delay;
    _delayed_task_start = emb::chrono::steady_clock::now();
  }

  static void run() {
    emb::chrono::milliseconds now = emb::chrono::steady_clock::now();

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
        _delayed_task_delay = emb::chrono::milliseconds(0);
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
