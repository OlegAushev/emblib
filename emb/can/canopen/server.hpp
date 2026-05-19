#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <span>

#include <emb/can.hpp>
#include <emb/concurrent/isr_spsc_inplace_queue.hpp>
#include <emb/delegate.hpp>

#include "can_transport.hpp"
#include "canopen_def.hpp"
#include "detail/emcy_producer.hpp"
#include "detail/hb_consumer.hpp"
#include "detail/hb_producer.hpp"
#include "detail/nmt_slave.hpp"
#include "detail/rpdo_consumer.hpp"
#include "detail/sdo_server.hpp"
#include "detail/sync_producer.hpp"
#include "detail/tpdo_producer.hpp"

namespace emb {
namespace canopen {

class server {
public:
  server(
      emb::delegate<std::chrono::milliseconds()> clock,
      can_transport& transport,
      node_id id,
      std::span<od_entry> dictionary
  );

  server(server const&) = delete;
  server& operator=(server const&) = delete;

  ~server() = default;

  void start();
  void stop();
  void run();

  node_id id() const {
    return id_;
  }
  nmt_state state() const {
    return nmt_.state();
  }

  // ---- RPDO ----

  void set_rpdo_handler(
      rpdo_num n,
      emb::delegate<void(emb::can::payload_t const&)> handler
  );

  void set_rpdo_timeout(
      rpdo_num n,
      std::chrono::milliseconds timeout,
      emb::delegate<void()> on_timeout
  );

  void set_rpdo_cob_id(rpdo_num n, emb::can::id_t custom_id);

  // ---- TPDO ----

  void set_tpdo_provider(
      tpdo_num n,
      emb::delegate<emb::can::payload_t()> provider
  );

  void set_tpdo_period(tpdo_num n, std::chrono::milliseconds period);

  // ---- heartbeat / sync / nmt ----

  void set_heartbeat_period(std::chrono::milliseconds period);
  void set_sync_period(std::chrono::milliseconds period);
  void on_nmt_change(emb::delegate<void(nmt_state)> handler);
  void on_reset_node(emb::delegate<void()> handler);
  void on_reset_communication(emb::delegate<void()> handler);

  bool watch_heartbeat(
      node_id remote,
      std::chrono::milliseconds timeout,
      emb::delegate<void(node_id)> on_lost
  );

  // ---- emcy ----

  bool emit_emcy(
      uint16_t error_code,
      uint8_t error_register,
      std::array<uint8_t, 5> manufacturer = {}
  );

private:
  static constexpr size_t rx_queue_capacity = 32;

  void enqueue_rx(emb::can::frame_t const& frame);
  void dispatch_rx(emb::can::frame_t const& frame);

  void apply_nmt_state(nmt_state s);
  void handle_nmt_command(emb::can::frame_t const& frame);

  emb::delegate<std::chrono::milliseconds()> clock_;

  can_transport& transport_;
  node_id id_;

  emb::isr_spsc_inplace_queue<emb::can::frame_t, rx_queue_capacity> rx_queue_;

  detail::nmt_slave nmt_;
  detail::hb_producer hb_producer_;
  detail::sync_producer sync_producer_;
  detail::hb_consumer hb_consumer_;
  detail::emcy_producer emcy_;
  detail::sdo_server sdo_;
  detail::tpdo_producer tpdo_;
  detail::rpdo_consumer rpdo_;

  emb::delegate<void(nmt_state)> on_nmt_change_;
  emb::delegate<void()> on_reset_node_;
  emb::delegate<void()> on_reset_communication_;
};

} // namespace canopen
} // namespace emb
