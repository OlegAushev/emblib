#pragma once

#include <emb/delegate.hpp>
#include <emb/container/inplace_vector.hpp>

#include <chrono>

namespace emb {
namespace scheduler {

template<typename Clock>
  requires std::chrono::is_clock_v<Clock>
class basic_scheduler {
public:
  class adaptive_task_context;

private:
  static constexpr std::size_t max_simple_tasks{16};
  static constexpr std::size_t max_adaptive_tasks{4};

  struct periodic_task {
    emb::delegate<void()> func;
    std::chrono::milliseconds period;
    Clock::time_point exec_timepoint;
  };

  struct adaptive_periodic_task {
    emb::delegate<void(adaptive_task_context)> func;
    std::chrono::milliseconds period;
    Clock::time_point exec_timepoint;
  };

  static inline emb::inplace_vector<periodic_task, max_simple_tasks> tasks_;
  static inline emb::inplace_vector<adaptive_periodic_task, max_adaptive_tasks>
      adaptive_tasks_;

  static inline Clock::time_point delayed_task_start_{};
  static inline std::chrono::milliseconds delayed_task_delay_{0};
  static inline emb::delegate<void()> delayed_task_;

public:
  // Valid only for the duration of the adaptive task callback — do not store.
  class adaptive_task_context {
    adaptive_periodic_task& self_;
  public:
    explicit adaptive_task_context(adaptive_periodic_task& p) : self_(p) {}
    void set_period(std::chrono::milliseconds p) {
      self_.period = p;
    }
    std::chrono::milliseconds period() const {
      return self_.period;
    }
  };

  basic_scheduler() = delete;

  static void add_task(emb::delegate<void()> task,
                       std::chrono::milliseconds period) {
    tasks_.push_back({task, period, Clock::now()});
  }

  static void
  add_adaptive_task(emb::delegate<void(adaptive_task_context)> task,
                    std::chrono::milliseconds period) {
    adaptive_tasks_.push_back({task, period, Clock::now()});
  }

  static void add_delayed_task(emb::delegate<void()> task,
                               std::chrono::milliseconds delay) {
    delayed_task_ = task;
    delayed_task_delay_ = delay;
    delayed_task_start_ = Clock::now();
  }

  static void run() {
    auto now = Clock::now();

    for (auto i = 0uz; i < tasks_.size(); ++i) {
      if (now >= tasks_[i].exec_timepoint + tasks_[i].period) {
        tasks_[i].exec_timepoint = now;
        tasks_[i].func();
      }
    }

    for (auto i = 0uz; i < adaptive_tasks_.size(); ++i) {
      if (now
          >= adaptive_tasks_[i].exec_timepoint + adaptive_tasks_[i].period) {
        adaptive_tasks_[i].exec_timepoint = now;
        adaptive_tasks_[i].func(adaptive_task_context(adaptive_tasks_[i]));
      }
    }

    if (delayed_task_delay_.count() != 0) {
      if (now >= delayed_task_start_ + delayed_task_delay_) {
        delayed_task_();
        delayed_task_delay_ = std::chrono::milliseconds(0);
      }
    }
  }

  static void reset() {
    auto now = Clock::now();
    for (auto i = 0uz; i < tasks_.size(); ++i) {
      tasks_[i].exec_timepoint = now;
    }
    for (auto i = 0uz; i < adaptive_tasks_.size(); ++i) {
      adaptive_tasks_[i].exec_timepoint = now;
    }
  }
};

} // namespace scheduler
} // namespace emb
