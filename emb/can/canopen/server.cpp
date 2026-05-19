#include "server.hpp"

#include <utility>

namespace emb {
namespace canopen {

server::server(
    emb::delegate<std::chrono::milliseconds()> clock,
    can_transport& transport,
    node_id id,
    std::span<od_entry> dictionary
)
    : clock_(clock),
      transport_(transport),
      id_(id),
      nmt_(id),
      hb_producer_(transport, id),
      sync_producer_(transport),
      hb_consumer_(transport),
      emcy_(transport, id),
      sdo_(transport, id, dictionary),
      tpdo_(transport, id),
      rpdo_(transport, id) {
  transport_.subscribe(emb::make_delegate<&server::enqueue_rx>(this));

  transport_.add_filter(nmt_.cob_id(), 0x7FF);
  apply_nmt_state(nmt_state::pre_operational);
}

void server::start() {
  apply_nmt_state(nmt_state::operational);
}
void server::stop() {
  apply_nmt_state(nmt_state::stopped);
}

void server::run() {
  auto now = clock_();

  while (auto frame = rx_queue_.try_pop()) {
    dispatch_rx(*frame);
  }

  uint8_t timed_out = rpdo_.tick(now, nmt_.state());
  for (size_t i = 0; i < 4; ++i) {
    if (timed_out & (1u << i)) emcy_.emit(0x8250, 0x10);
  }

  tpdo_.tick(now, nmt_.state());
  sdo_.drain();
  hb_producer_.tick(now, nmt_.state());
  sync_producer_.tick(now);
  hb_consumer_.tick(now);
}

void server::enqueue_rx(emb::can::frame_t const& frame) {
  (void)rx_queue_.try_push(frame); // drop-newest on full
}

void server::dispatch_rx(emb::can::frame_t const& frame) {
  if (nmt_.match(frame)) {
    handle_nmt_command(frame);
    return;
  }
  if (sdo_.try_handle(frame)) return;
  auto now = clock_();
  if (rpdo_.try_handle(frame, now, nmt_.state())) return;
  hb_consumer_.try_handle(frame, now);
}

void server::handle_nmt_command(emb::can::frame_t const& frame) {
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

void server::apply_nmt_state(nmt_state s) {
  if (s == nmt_.state()) return;
  nmt_.set_state(s);
  auto now = clock_();
  if (s == nmt_state::operational) rpdo_.reset_timers(now);
  if (on_nmt_change_) on_nmt_change_(s);
}

// ---- delegating API ----

void server::set_rpdo_handler(
    rpdo_num n,
    emb::delegate<void(emb::can::payload_t const&)> handler
) {
  rpdo_.set_handler(n, handler);
}

void server::set_rpdo_timeout(
    rpdo_num n,
    std::chrono::milliseconds timeout,
    emb::delegate<void()> on_timeout
) {
  auto now = clock_();
  rpdo_.set_timeout(n, timeout, on_timeout, now);
}

void server::set_rpdo_cob_id(rpdo_num n, emb::can::id_t custom_id) {
  rpdo_.set_cob_id(n, custom_id);
}

void server::set_tpdo_provider(
    tpdo_num n,
    emb::delegate<emb::can::payload_t()> provider
) {
  tpdo_.set_provider(n, provider);
}

void server::set_tpdo_period(tpdo_num n, std::chrono::milliseconds period) {
  auto now = clock_();
  tpdo_.set_period(n, period, now);
}

void server::set_heartbeat_period(std::chrono::milliseconds period) {
  auto now = clock_();
  hb_producer_.set_period(period, now);
}

void server::set_sync_period(std::chrono::milliseconds period) {
  auto now = clock_();
  sync_producer_.set_period(period, now);
}

void server::on_nmt_change(emb::delegate<void(nmt_state)> func) {
  on_nmt_change_ = func;
}

void server::on_reset_node(emb::delegate<void()> func) {
  on_reset_node_ = func;
}

void server::on_reset_communication(emb::delegate<void()> func) {
  on_reset_communication_ = func;
}

bool server::watch_heartbeat(
    node_id remote,
    std::chrono::milliseconds timeout,
    emb::delegate<void(node_id)> on_lost
) {
  auto now = clock_();
  return hb_consumer_.watch(remote, timeout, on_lost, now);
}

bool server::emit_emcy(
    uint16_t error_code,
    uint8_t error_register,
    std::array<uint8_t, 5> manufacturer
) {
  return emcy_.emit(error_code, error_register, manufacturer);
}

} // namespace canopen
} // namespace emb
