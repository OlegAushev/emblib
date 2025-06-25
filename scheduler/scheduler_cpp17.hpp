#pragma once

#if __cplusplus >= 201700

#include <emblib/chrono.hpp>
#include <emblib/core.hpp>
#include <emblib/static_vector.hpp>

namespace emb {
namespace scheduler {

enum class task_execstatus {
  success,
  fail
};

class basic_scheduler {
private:
  static inline bool initialized_{false};
  static constexpr size_t max_taskcount{8};

  struct task {
    std::chrono::milliseconds period;
    std::chrono::time_point<emb::chrono::steady_clock> timepoint;
    task_execstatus (*func)(size_t);
  };

  static inline emb::static_vector<task, max_taskcount> tasks_;

  static task_execstatus empty_task(size_t) { return task_execstatus::success; }

private:
  static inline std::chrono::time_point<emb::chrono::steady_clock>
      delayed_task_start_{};
  static inline std::chrono::milliseconds delayed_task_delay_{0};

  static void empty_delayed_task() {}

  static inline void (*delayed_task_)() = empty_delayed_task;

public:
  basic_scheduler() = delete;

  static void init() { initialized_ = true; }

  static void add_task(task_execstatus (*func)(size_t),
                       std::chrono::milliseconds period) {
    task task_ = {period, emb::chrono::steady_clock::now(), func};
    tasks_.push_back(task_);
  }

  static void set_task_period(size_t index, std::chrono::milliseconds period) {
    if (index < tasks_.size()) {
      tasks_[index].period = period;
    }
  }

  static void add_delayed_task(void (*func)(),
                               std::chrono::milliseconds delay) {
    delayed_task_ = func;
    delayed_task_delay_ = delay;
    delayed_task_start_ = emb::chrono::steady_clock::now();
  }

  static void run() {
    auto now = emb::chrono::steady_clock::now();

    for (size_t i = 0; i < tasks_.size(); ++i) {
      if (now >= (tasks_[i].timepoint + tasks_[i].period)) {
        if (tasks_[i].func(i) == task_execstatus::success) {
          tasks_[i].timepoint = now;
        }
      }
    }

    if (delayed_task_delay_.count() != 0) {
      if (now >= (delayed_task_start_ + delayed_task_delay_)) {
        delayed_task_();
        delayed_task_delay_ = std::chrono::milliseconds(0);
      }
    }
  }

  static void reset() {
    for (size_t i = 0; i < tasks_.size(); ++i) {
      tasks_[i].timepoint = emb::chrono::steady_clock::now();
    }
  }
};

} // namespace scheduler
} // namespace emb

#endif
