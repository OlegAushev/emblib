#pragma once

#include <emb/chrono.hpp>
#include <emb/core.hpp>
#include <emb/delegate.hpp>
#include <emb/static_vector.hpp>

namespace emb {
namespace scheduler {

class basic_scheduler {
public:
  class adaptive_task_context;

private:
  static constexpr size_t max_simple_tasks{16};
  static constexpr size_t max_adaptive_tasks{4};

  struct periodic_task {
    emb::delegate<void()> func;
    std::chrono::milliseconds period;
    std::chrono::time_point<emb::chrono::steady_clock> exec_timepoint;
  };

  struct adaptive_periodic_task {
    emb::delegate<void(adaptive_task_context)> func;
    std::chrono::milliseconds period;
    std::chrono::time_point<emb::chrono::steady_clock> exec_timepoint;
  };

  static inline emb::static_vector<periodic_task, max_simple_tasks> tasks_;
  static inline emb::static_vector<adaptive_periodic_task, max_adaptive_tasks>
      adaptive_tasks_;

  static inline std::chrono::time_point<emb::chrono::steady_clock>
      delayed_task_start_{};
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

  template<auto F>
  static void add_task(std::chrono::milliseconds period) {
    tasks_.push_back(
        {emb::delegate<void()>::template bind<F>(),
         period,
         emb::chrono::steady_clock::now()}
    );
  }

  template<auto M, typename T>
  static void add_task(T* obj, std::chrono::milliseconds period) {
    tasks_.push_back(
        {emb::delegate<void()>::template bind<M>(obj),
         period,
         emb::chrono::steady_clock::now()}
    );
  }

  template<auto M, typename T>
  static void add_task(T const* obj, std::chrono::milliseconds period) {
    tasks_.push_back(
        {emb::delegate<void()>::template bind<M>(obj),
         period,
         emb::chrono::steady_clock::now()}
    );
  }

  template<auto F>
  static void add_adaptive_task(std::chrono::milliseconds period) {
    adaptive_tasks_.push_back(
        {emb::delegate<void(adaptive_task_context)>::template bind<F>(),
         period,
         emb::chrono::steady_clock::now()}
    );
  }

  template<auto M, typename T>
  static void add_adaptive_task(T* obj, std::chrono::milliseconds period) {
    adaptive_tasks_.push_back(
        {emb::delegate<void(adaptive_task_context)>::template bind<M>(obj),
         period,
         emb::chrono::steady_clock::now()}
    );
  }

  template<auto M, typename T>
  static void
  add_adaptive_task(T const* obj, std::chrono::milliseconds period) {
    adaptive_tasks_.push_back(
        {emb::delegate<void(adaptive_task_context)>::template bind<M>(obj),
         period,
         emb::chrono::steady_clock::now()}
    );
  }

  template<auto F>
  static void add_delayed_task(std::chrono::milliseconds delay) {
    delayed_task_ = emb::delegate<void()>::template bind<F>();
    delayed_task_delay_ = delay;
    delayed_task_start_ = emb::chrono::steady_clock::now();
  }

  template<auto M, typename T>
  static void add_delayed_task(T* obj, std::chrono::milliseconds delay) {
    delayed_task_ = emb::delegate<void()>::template bind<M>(obj);
    delayed_task_delay_ = delay;
    delayed_task_start_ = emb::chrono::steady_clock::now();
  }

  template<auto M, typename T>
  static void add_delayed_task(T const* obj,
                               std::chrono::milliseconds delay) {
    delayed_task_ = emb::delegate<void()>::template bind<M>(obj);
    delayed_task_delay_ = delay;
    delayed_task_start_ = emb::chrono::steady_clock::now();
  }

  static void run() {
    auto now = emb::chrono::steady_clock::now();

    for (size_t i = 0; i < tasks_.size(); ++i) {
      if (now >= tasks_[i].exec_timepoint + tasks_[i].period) {
        tasks_[i].exec_timepoint = now;
        tasks_[i].func();
      }
    }

    for (size_t i = 0; i < adaptive_tasks_.size(); ++i) {
      if (now
          >= adaptive_tasks_[i].exec_timepoint + adaptive_tasks_[i].period) {
        adaptive_tasks_[i].exec_timepoint = now;
        adaptive_tasks_[i].func(adaptive_task_context{adaptive_tasks_[i]});
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
    auto now = emb::chrono::steady_clock::now();
    for (size_t i = 0; i < tasks_.size(); ++i) {
      tasks_[i].exec_timepoint = now;
    }
    for (size_t i = 0; i < adaptive_tasks_.size(); ++i) {
      adaptive_tasks_[i].exec_timepoint = now;
    }
  }
};

} // namespace scheduler
} // namespace emb
