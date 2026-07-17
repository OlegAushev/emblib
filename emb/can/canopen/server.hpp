#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>

#include <emb/can.hpp>
#include <emb/can/bus.hpp>
#include <emb/concurrent/isr_spsc_inplace_queue.hpp>
#include <emb/delegate.hpp>

#include "detail/emcy_producer.hpp"
#include "detail/hb_consumer.hpp"
#include "detail/hb_producer.hpp"
#include "detail/nmt_slave.hpp"
#include "detail/rpdo_consumer.hpp"
#include "detail/sdo_server.hpp"
#include "detail/sync_producer.hpp"
#include "detail/tpdo_producer.hpp"
#include "od.hpp"
#include "types.hpp"

namespace emb {
namespace can {
namespace canopen {

struct server_options {
  std::uint8_t node_id;
  std::size_t tpdo_count = 4;
  std::size_t rpdo_count = 4;
  std::size_t rx_queue_capacity = 32;
};

template<server_options Opt>
class server {
  static_assert(valid_node_id<Opt.node_id>, "node id must be in [1, 127]");

public:
  server(
      emb::delegate<std::chrono::milliseconds()> clock,
      transport& bus,
      std::span<od_entry> dictionary
  )
      : clock_(clock),
        bus_(bus),
        hb_producer_(bus),
        sync_producer_(bus),
        hb_consumer_(bus),
        emcy_(bus),
        sdo_(bus, dictionary),
        tpdo_(bus),
        rpdo_(bus) {
    bus_.subscribe(emb::make_delegate<&server::enqueue_rx>(this));

    bus_.add_filter(format_t::standard, nmt_.cob_id(), 0x7FF);
    apply_nmt_state(nmt_state::pre_operational);
  }

  server(server const&) = delete;
  server& operator=(server const&) = delete;

  ~server() = default;

  static constexpr server_options options() {
    return Opt;
  }

  void start() {
    apply_nmt_state(nmt_state::operational);
  }
  void stop() {
    apply_nmt_state(nmt_state::stopped);
  }

  void run() {
    auto now = clock_();

    while (auto frame = rx_queue_.try_pop()) {
      dispatch_rx(*frame);
    }

    std::size_t timed_out = rpdo_.tick(now, nmt_.state());
    for (std::size_t i = 0; i < timed_out; ++i) {
      emcy_.emit(0x8250, 0x10);
    }

    tpdo_.tick(now, nmt_.state());
    sdo_.drain();
    hb_producer_.tick(now, nmt_.state());
    sync_producer_.tick(now);
    hb_consumer_.tick(now);
  }

  nmt_state state() const {
    return nmt_.state();
  }

  // ---- RPDO ----

  template<std::size_t I, pdo_id Cob = pdo_id::predefined()>
  void setup_rpdo(rpdo_config cfg) {
    static_assert(I >= 1 && I <= Opt.rpdo_count, "RPDO index out of range");
    static_assert(
        Cob.is_custom || I <= 4,
        "PDO >= 5 requires pdo_id::custom(...)"
    );
    static_assert(
        !Cob.is_custom || (Cob.value >= 1 && Cob.value <= 0x7FF),
        "COB-ID out of 11-bit range"
    );
    rpdo_.template setup<I, Cob>(cfg, clock_());
  }

  // ---- TPDO ----

  template<std::size_t I, pdo_id Cob = pdo_id::predefined()>
  void setup_tpdo(tpdo_config cfg) {
    static_assert(I >= 1 && I <= Opt.tpdo_count, "TPDO index out of range");
    static_assert(
        Cob.is_custom || I <= 4,
        "PDO >= 5 requires pdo_id::custom(...)"
    );
    static_assert(
        !Cob.is_custom || (Cob.value >= 1 && Cob.value <= 0x7FF),
        "COB-ID out of 11-bit range"
    );
    tpdo_.template setup<I, Cob>(cfg, clock_());
  }

  // ---- heartbeat / sync / nmt ----

  void set_heartbeat_period(std::chrono::milliseconds period) {
    auto now = clock_();
    hb_producer_.set_period(period, now);
  }

  void set_sync_period(std::chrono::milliseconds period) {
    auto now = clock_();
    sync_producer_.set_period(period, now);
  }

  void on_nmt_change(emb::delegate<void(nmt_state)> handler) {
    on_nmt_change_ = handler;
  }

  void on_reset_node(emb::delegate<void()> handler) {
    on_reset_node_ = handler;
  }

  void on_reset_communication(emb::delegate<void()> handler) {
    on_reset_communication_ = handler;
  }

  template<std::uint8_t Remote>
  bool watch_heartbeat(
      std::chrono::milliseconds timeout,
      emb::delegate<void(std::uint8_t)> on_lost
  ) {
    auto now = clock_();
    return hb_consumer_.watch<Remote>(timeout, on_lost, now);
  }

  // ---- emcy ----

  bool emit_emcy(
      std::uint16_t error_code,
      std::uint8_t error_register,
      std::array<std::uint8_t, 5> manufacturer = {}
  ) {
    return emcy_.emit(error_code, error_register, manufacturer);
  }

private:
  void enqueue_rx(frame_t const& frame) {
    (void)rx_queue_.try_push(frame); // drop-newest on full
  }

  void dispatch_rx(frame_t const& frame) {
    if (nmt_.match(frame)) {
      handle_nmt_command(frame);
      return;
    }
    if (sdo_.try_handle(frame)) return;
    auto now = clock_();
    if (rpdo_.try_handle(frame, now, nmt_.state())) return;
    hb_consumer_.try_handle(frame, now);
  }

  void apply_nmt_state(nmt_state s) {
    if (s == nmt_.state()) return;
    nmt_.set_state(s);
    auto now = clock_();
    if (s == nmt_state::operational) rpdo_.reset_timers(now);
    if (on_nmt_change_) on_nmt_change_(s);
  }

  void handle_nmt_command(frame_t const& frame) {
    auto cmd = nmt_.decode(frame);
    if (!cmd) return;

    switch (*cmd) {
    case nmt_command::start: apply_nmt_state(nmt_state::operational); break;
    case nmt_command::stop: apply_nmt_state(nmt_state::stopped); break;
    case nmt_command::enter_pre_operational:
      apply_nmt_state(nmt_state::pre_operational);
      break;
    case nmt_command::reset_node:
      if (on_reset_node_) on_reset_node_();
      apply_nmt_state(nmt_state::pre_operational);
      break;
    case nmt_command::reset_communication:
      if (on_reset_communication_) on_reset_communication_();
      apply_nmt_state(nmt_state::pre_operational);
      break;
    }
  }

  emb::delegate<std::chrono::milliseconds()> clock_;

  transport& bus_;

  emb::isr_spsc_inplace_queue<frame_t, Opt.rx_queue_capacity> rx_queue_;

  detail::nmt_slave<Opt.node_id> nmt_;
  detail::hb_producer<Opt.node_id> hb_producer_;
  detail::sync_producer sync_producer_;
  detail::hb_consumer hb_consumer_;
  detail::emcy_producer<Opt.node_id> emcy_;
  detail::sdo_server<Opt.node_id> sdo_;
  detail::tpdo_producer<Opt.node_id, Opt.tpdo_count> tpdo_;
  detail::rpdo_consumer<Opt.node_id, Opt.rpdo_count> rpdo_;

  emb::delegate<void(nmt_state)> on_nmt_change_;
  emb::delegate<void()> on_reset_node_;
  emb::delegate<void()> on_reset_communication_;
};

} // namespace canopen
} // namespace can
} // namespace emb
