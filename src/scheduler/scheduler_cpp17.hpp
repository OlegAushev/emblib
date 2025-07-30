#pragma once

#if __cplusplus >= 201700

#include <emb/chrono.hpp>
#include <emb/core.hpp>
#include <emb/static_vector.hpp>

namespace emb {
namespace scheduler {

class basic_scheduler {
public:
  using periodic_task_type = void (*)(size_t);
  using onetime_task_type = void (*)();
private:
  static constexpr size_t max_taskcount{16};

  struct periodic_task {
    periodic_task_type func;
    std::chrono::milliseconds period;
    std::chrono::time_point<emb::chrono::steady_clock> exec_timepoint;
  };

  struct onetime_task {
    onetime_task_type func;
    std::chrono::milliseconds delay;
    std::chrono::time_point<emb::chrono::steady_clock> creation_timepoint;
  };

  static inline emb::static_vector<periodic_task, max_taskcount> tasks_;

  static void empty_task(size_t) {}

private:
  static inline std::chrono::time_point<emb::chrono::steady_clock>
      delayed_task_start_{};
  static inline std::chrono::milliseconds delayed_task_delay_{0};

  static void empty_delayed_task() {}

  static inline void (*delayed_task_)() = empty_delayed_task;

public:
  basic_scheduler() = delete;

  static void add_task(periodic_task_type func,
                       std::chrono::milliseconds period) {
    periodic_task task_ = {func, period, emb::chrono::steady_clock::now()};
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
      if (now >= (tasks_[i].exec_timepoint + tasks_[i].period)) {
        tasks_[i].func(i);
        tasks_[i].exec_timepoint = now;
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
      tasks_[i].exec_timepoint = emb::chrono::steady_clock::now();
    }
  }
};

} // namespace scheduler
} // namespace emb

#endif
