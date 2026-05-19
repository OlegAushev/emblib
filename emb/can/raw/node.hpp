#pragma once

#include <chrono>
#include <cstdint>

#include <emb/assert.hpp>
#include <emb/can.hpp>
#include <emb/can/bus.hpp>
#include <emb/concurrent/isr_spsc_inplace_queue.hpp>
#include <emb/container/inplace_vector.hpp>
#include <emb/delegate.hpp>

namespace emb {
namespace can {
namespace raw {

template<size_t RxSlots = 8, size_t TxSlots = 8, size_t RxQueueCapacity = 32>
class node {
public:
  explicit node(transport& bus) : bus_(bus) {
    bus_.subscribe(emb::make_delegate<&node::enqueue_rx>(this));
  }

  node(node const&) = delete;
  node& operator=(node const&) = delete;

  void add_rx(
      format_t format,
      id_t id,
      id_t mask,
      std::chrono::milliseconds timeout,
      emb::delegate<void(frame_t const&)> handler,
      emb::delegate<void()> on_timeout
  ) {
    bool ok = rx_.try_push_back(
        {.format = format,
         .id = id,
         .mask = mask,
         .timeout = timeout,
         .last_rx = now_,
         .timed_out = false,
         .handler = handler,
         .on_timeout = on_timeout}
    );
    emb::ensure(ok);
    bus_.add_filter(format, id, mask);
  }

  void add_periodic_tx(
      format_t format,
      id_t id,
      uint8_t len,
      std::chrono::milliseconds period,
      emb::delegate<payload_t()> provider
  ) {
    bool ok = tx_.try_push_back(
        {.format = format,
         .id = id,
         .len = len,
         .period = period,
         .last_tx = now_,
         .provider = provider}
    );
    emb::ensure(ok);
  }

  void run(std::chrono::milliseconds since_boot) {
    now_ = since_boot;

    while (auto frame = rx_queue_.try_pop()) {
      dispatch_rx(*frame);
    }

    for (auto& s : rx_) {
      if (s.timeout == std::chrono::milliseconds::zero() || s.timed_out) {
        continue;
      }
      if ((now_ - s.last_rx) >= s.timeout) {
        s.timed_out = true;
        if (s.on_timeout) s.on_timeout();
      }
    }

    for (auto& s : tx_) {
      if (s.period == std::chrono::milliseconds::zero() || !s.provider) {
        continue;
      }
      if ((now_ - s.last_tx) < s.period) continue;

      frame_t frame = {
          .format = s.format,
          .id = s.id,
          .len = s.len,
          .payload = s.provider()
      };

      if (bus_.send(frame)) {
        s.last_tx = now_;
      }
    }
  }

private:
  struct rx_slot {
    format_t format = format_t::standard;
    id_t id = 0;
    id_t mask = 0;
    std::chrono::milliseconds timeout{0};
    std::chrono::milliseconds last_rx{0};
    bool timed_out = false;
    emb::delegate<void(frame_t const&)> handler;
    emb::delegate<void()> on_timeout;
  };

  struct tx_slot {
    format_t format = format_t::standard;
    id_t id = 0;
    uint8_t len = 0;
    std::chrono::milliseconds period{0};
    std::chrono::milliseconds last_tx{0};
    emb::delegate<payload_t()> provider;
  };

  void enqueue_rx(frame_t const& frame) {
    (void)rx_queue_.try_push(frame); // drop-newest on full
  }

  void dispatch_rx(frame_t const& frame) {
    for (auto& s : rx_) {
      if (frame.format != s.format) continue;
      if ((frame.id & s.mask) != (s.id & s.mask)) continue;
      s.last_rx = now_;
      s.timed_out = false;
      if (s.handler) s.handler(frame);
      return;
    }
  }

  transport& bus_;
  std::chrono::milliseconds now_{0};
  emb::inplace_vector<rx_slot, RxSlots> rx_;
  emb::inplace_vector<tx_slot, TxSlots> tx_;
  emb::isr_spsc_inplace_queue<frame_t, RxQueueCapacity> rx_queue_;
};

} // namespace raw
} // namespace can
} // namespace emb
