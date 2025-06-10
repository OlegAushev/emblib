#pragma once

#include <cstdio>
#include <cstring>
#include <emblib/chrono.hpp>
#include <emblib/core.hpp>

namespace emb {

#if defined(EMBLIB_C28X)

class duration_logger {
private:
  static emb::chrono::nanoseconds (*_time_now_func)();
  static size_t const _message_len_max = 32;
  char _message[_message_len_max];
  uint64_t volatile _start;
public:
  explicit duration_logger(char const* message) {
    strncpy(_message, message, _message_len_max - 1);
    _message[_message_len_max - 1] = '\0';
    _start = _time_now_func().count();
  }

  ~duration_logger() {
    uint64_t volatile finish = _time_now_func().count();
    if (finish < _start) {
      printf("%s: timer overflow\n", _message);
    } else {
      printf("%s: %.3f us\n", _message, float(finish - _start) / 1000.f);
    }
  }

  static void init(emb::chrono::nanoseconds (*time_now_func)(void)) {
    _time_now_func = time_now_func;
  }
};

#define EMB_LOG_DURATION_us(message) \
  volatile emb::duration_logger EMB_UNIQ_ID(__LINE__)(message);

class duration_logger_clk {
private:
  static uint32_t (*_time_now_func)();
  static size_t const _message_len_max = 32;
  char _message[_message_len_max];
  uint32_t volatile _start;
public:
  explicit duration_logger_clk(char const* message) {
    strncpy(_message, message, _message_len_max - 1);
    _message[_message_len_max - 1] = '\0';
    _start = _time_now_func();
  }

  ~duration_logger_clk() {
    uint32_t volatile finish = _time_now_func();
    if (finish > _start) {
      printf("%s: timer overflow\n", _message);
    } else {
      printf("%s: %lu clock cycles\n", _message, _start - finish);
    }
  }

  static void init(uint32_t (*time_now_func_clk)(void)) {
    _time_now_func = time_now_func_clk;
  }
};

#define EMB_LOG_DURATION_clk(message) \
  volatile emb::duration_logger_clk EMB_UNIQ_ID(__LINE__)(message);

class duration_logger_async {
private:
  static emb::chrono::nanoseconds (*_time_now_func)();
  static size_t const _capacity = 10;

  struct DurationData {
    char const* message;
    float volatile value;

    DurationData() : value(0) {}
  };

  static DurationData _durations_us[_capacity];
  size_t const _channel;
  uint64_t volatile _start;
public:
  duration_logger_async(char const* message, size_t channel)
      : _channel(channel) {
    _durations_us[_channel].message = message;
    _start = _time_now_func().count();
  }

  ~duration_logger_async() {
    uint64_t volatile finish = _time_now_func().count();
    if (finish < _start) {
      _durations_us[_channel].value = 0;
    } else {
      _durations_us[_channel].value = float(finish - _start) / 1000.f;
    }
  }

  static void init(emb::chrono::nanoseconds (*time_now_func)(void)) {
    _time_now_func = time_now_func;
  }

  static void print() {
    for (size_t i = 0; i < _capacity; ++i) {
      if (_durations_us[i].value != 0) {
        printf(
            "%s: %.3f us\n", _durations_us[i].message, _durations_us[i].value);
      }
    }
  }
};

#define EMB_LOG_DURATION_ASYNC_us(message, channel) \
  volatile emb::duration_logger_async EMB_UNIQ_ID(__LINE__)(message, channel);

#endif

} // namespace emb
