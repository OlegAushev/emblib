#pragma once

#include <array>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <optional>
#include <type_traits>

#include <emb/can.hpp>
#include <emb/can/bus.hpp>
#include <emb/concurrent/spsc_queue.hpp>
#include <emb/delegate.hpp>

#include "detail/emcy_producer.hpp"
#include "detail/hb_consumer.hpp"
#include "detail/hb_producer.hpp"
#include "detail/nmt_slave.hpp"
#include "detail/rpdo_consumer.hpp"
#include "detail/sdo_server.hpp"
#include "detail/sync_producer.hpp"
#include "detail/tpdo_producer.hpp"
#include "mpdo.hpp"
#include "od.hpp"
#include "od_dictionary.hpp"
#include "od_process_data.hpp"
#include "pdo.hpp"
#include "types.hpp"

namespace emb {
namespace can {
namespace canopen {

namespace detail {

// Whether `Provider` takes a `T*` and returns a `Message`.
template<auto Provider, typename T, typename Message>
inline constexpr bool provides = [] {
  if constexpr (std::is_invocable_v<decltype(Provider), T*>) {
    return std::same_as<
        std::remove_cvref_t<std::invoke_result_t<decltype(Provider), T*>>,
        Message>;
  }
  else {
    return false;
  }
}();

// Returns the payload of a frame of the TPDO `Mapped`, a `mapped_pdo` of rows,
// for the counter of frames `counter`: the message that the provider
// `Provider` of `obj` returns, its counter, if the PDO has one, set to
// `counter`, encoded by the rows of the PDO (`encode_pdo`). The setter of the
// counter is a constant of the thunk, as the rows themselves are of
// `encode_pdo`: at -O0 a reference to the rows would put them, names, labels
// and all, in the image.
template<auto const& Mapped, auto Provider, typename T>
constexpr std::optional<payload_t> described_next(T* obj, std::uint8_t counter)
{
  auto message = std::invoke(Provider, obj);
  constexpr auto row = pdo_counter_row<mapped_list<Mapped>>;
  if constexpr (row < std::size(mapped_list<Mapped>)) {
    constexpr auto set = mapped_list<Mapped>[row].codec.set;
    set(message, counter);
  }
  return encode_pdo<mapped_list<Mapped>>(message);
}

// Hands `payload`, a frame of the RPDO `Mapped`, a `mapped_pdo` of rows, to
// the handler `Handler` of `obj` as a `received_pdo` of the rows of the PDO.
template<auto const& Mapped, auto Handler, typename T>
constexpr void described_handle(T* obj, payload_t const& payload)
{
  std::invoke(Handler, obj, received_pdo<mapped_list<Mapped>>{payload});
}

// Returns the source of the slot of the TPDO `Mapped` whose provider is
// `Provider` of `obj`: `described_next` bound to `obj`, and the mask of the
// counter of the PDO.
template<auto const& Mapped, auto Provider, typename T>
constexpr tpdo_source described_source(T& obj)
{
  return {
      .next =
          emb::delegate<std::optional<payload_t>(std::uint8_t)>::template bind<
              &described_next<Mapped, Provider, T>>(&obj),
      .sent = {},
      .counter_mask = counter_mask_of<mapped_list<Mapped>>};
}

// Returns the handler of the slot of the RPDO `Mapped` whose handler is
// `Handler` of `obj`: `described_handle` bound to `obj`.
template<auto const& Mapped, auto Handler, typename T>
constexpr emb::delegate<void(payload_t const&)> described_handler(T& obj)
{
  return emb::delegate<void(payload_t const&)>::template bind<
      &described_handle<Mapped, Handler, T>>(&obj);
}

// Returns the frame that `scanner`, the source of an MPDO in source address
// mode, gives this period (`mpdo_scanner::next`). An MPDO has no counter, and
// the thunk takes the slot's counter of frames, which `next` does not, and
// drops it.
template<typename Ctx, auto const& Scans>
constexpr std::optional<payload_t>
scanned_next(mpdo_scanner<Ctx, Scans>* scanner, std::uint8_t)
{
  return scanner->next();
}

// Returns the source of the slot of an MPDO in source address mode whose
// objects `scanner` gives: `scanned_next` and `mpdo_scanner::sent` bound to
// it, and no counter, its mask 0.
template<typename Ctx, auto const& Scans>
constexpr tpdo_source scanned_source(mpdo_scanner<Ctx, Scans>& scanner)
{
  using scanner_type = mpdo_scanner<Ctx, Scans>;
  return {
      .next = emb::delegate<std::optional<payload_t>(
          std::uint8_t)>::template bind<&scanned_next<Ctx, Scans>>(&scanner),
      .sent =
          emb::delegate<void()>::template bind<&scanner_type::sent>(&scanner),
      .counter_mask = 0};
}

} // namespace detail

struct server_options {
  std::size_t tpdo_count = 4;
  std::size_t rpdo_count = 4;
  std::size_t rx_queue_capacity = 32;
};

template<server_options Opt, typename Ctx>
class server {
public:
  server(emb::delegate<std::chrono::milliseconds()> clock,
         transport& bus,
         node_id node,
         od_view<Ctx> dictionary,
         Ctx& ctx)
      : clock_(clock),
        bus_(bus),
        nmt_(node),
        hb_producer_(bus, node),
        sync_producer_(bus),
        hb_consumer_(bus),
        emcy_(bus, node),
        sdo_(bus, node, dictionary, ctx),
        tpdo_(bus, node),
        rpdo_(bus, node)
  {
    bus_.subscribe(emb::make_delegate<&server::enqueue_rx>(this));

    bus_.add_filter(format_t::standard, nmt_.cob_id(), 0x7FF);
    apply_nmt_state(nmt_state::pre_operational);
  }

  server(emb::delegate<std::chrono::milliseconds()> clock,
         transport& bus,
         node_id node,
         od_view<Ctx> dictionary,
         Ctx&& ctx) = delete;

  server(server const&) = delete;
  server& operator=(server const&) = delete;

  ~server() = default;

  static constexpr server_options options()
  {
    return Opt;
  }

  void start()
  {
    apply_nmt_state(nmt_state::operational);
  }
  void stop()
  {
    apply_nmt_state(nmt_state::stopped);
  }

  void run()
  {
    auto now = clock_();

    while (auto frame = rx_queue_.try_pop()) {
      dispatch_rx(*frame);
    }

    std::size_t timed_out = rpdo_.tick(now, nmt_.state());
    for (std::size_t i = 0; i < timed_out; ++i) {
      emcy_.emit(emcy_code::rpdo_timeout,
                 error_register::generic | error_register::communication);
    }

    tpdo_.tick(now, nmt_.state());
    sdo_.drain();
    hb_producer_.tick(now, nmt_.state());
    sync_producer_.tick(now);
    hb_consumer_.tick(now);
  }

  nmt_state state() const
  {
    return nmt_.state();
  }

  // ---- RPDO ----

  template<std::size_t I, pdo_id CobId = pdo_id::predefined()>
  void setup_rpdo(rpdo_config cfg)
  {
    static_assert(I >= 1 && I <= Opt.rpdo_count, "RPDO index out of range");
    check_cob_id<I, CobId>();
    rpdo_.template setup<I, CobId>(cfg, clock_());
  }

  // Sets the slot of the RPDO `Mapped` up from its description: the
  // `mapped_pdo` of the PDO and its rows that `pdo_of` makes, the constant
  // that the description of the dictionary takes too. The slot is that of the
  // number of the PDO, with its COB-ID and its timeout; it hands each frame
  // of 8 bytes on its COB-ID to `Handler`, a member function of `obj` that
  // takes a `received_pdo` of the rows of the PDO, and calls `on_timeout`, if
  // set, when the RPDO times out. The program is ill-formed if the PDO is no
  // RPDO, if its number is past the slots or its COB-ID one that
  // `setup_rpdo<I, CobId>` refuses, or if `Handler` takes the frames of
  // another PDO. The number, the COB-ID and the timeout are read from
  // `Mapped.pdo` in constant expressions only, so that neither the PDO nor
  // its rows reach the image, and the slot is written whole, as by any
  // `setup_rpdo`.
  template<auto const& Mapped, auto Handler, typename T>
    requires some_pdo_rows<std::remove_cvref_t<decltype(mapped_list<Mapped>)>>
  void setup_rpdo(T& obj, emb::delegate<void()> on_timeout = {})
  {
    static_assert(Mapped.pdo.direction == pdo_direction::rx,
                  "setup_rpdo takes a PDO that the device receives");
    static_assert(std::is_invocable_v<decltype(Handler),
                                      T*,
                                      received_pdo<mapped_list<Mapped>> const&>,
                  "the handler takes the frames of the PDO, a received_pdo "
                  "of its rows");
    constexpr auto number = Mapped.pdo.number;
    constexpr auto cob_id = Mapped.pdo.cob_id;
    constexpr auto timeout = Mapped.pdo.timeout;
    static_assert(number >= 1 && number <= Opt.rpdo_count,
                  "RPDO index out of range");
    check_cob_id<number, cob_id>();
    rpdo_.template setup<number, cob_id>(
        {.handler = detail::described_handler<Mapped, Handler>(obj),
         .timeout = timeout,
         .on_timeout = on_timeout},
        clock_());
  }

  // binding to a temporary would leave the slot with a dangling pointer
  template<auto const& Mapped, auto Handler, typename T>
  void setup_rpdo(T const&&, emb::delegate<void()> = {}) = delete;

  // ---- TPDO ----

  template<std::size_t I, pdo_id CobId = pdo_id::predefined()>
  void setup_tpdo(tpdo_config cfg)
  {
    static_assert(I >= 1 && I <= Opt.tpdo_count, "TPDO index out of range");
    check_cob_id<I, CobId>();
    tpdo_.template setup<I, CobId>(cfg, clock_());
  }

  // Sets the slot of the TPDO `Mapped` up from its description: the
  // `mapped_pdo` of the PDO and its rows that `pdo_of` makes, the constant
  // that the description of the dictionary takes too. The slot is that of the
  // number of the PDO, with its COB-ID and its period. Once a period it
  // calls `Provider`, a member function of `obj` that returns the message of
  // the PDO, sets the counter of the message, if the PDO has one, to the
  // slot's counter of frames, and sends the message encoded by the rows of
  // the PDO (`encode_pdo`); the counter steps, modulo the width of its signal,
  // only when `send` takes a frame. The program is ill-formed if the PDO is
  // no TPDO, if its number is past the slots or its COB-ID one that
  // `setup_tpdo<I, CobId>` refuses, or if `Provider` returns another message
  // than that of the PDO. The number, the COB-ID and the period are read from
  // `Mapped.pdo` in constant expressions only, so that neither the PDO nor
  // its rows reach the image, and the slot is written whole, as by any
  // `setup_tpdo`.
  template<auto const& Mapped, auto Provider, typename T>
    requires some_pdo_rows<std::remove_cvref_t<decltype(mapped_list<Mapped>)>>
  void setup_tpdo(T& obj)
  {
    static_assert(Mapped.pdo.direction == pdo_direction::tx,
                  "setup_tpdo takes a PDO that the device sends");
    static_assert(detail::provides<Provider, T, mapped_message_t<Mapped>>,
                  "the provider returns the message of the PDO");
    constexpr auto number = Mapped.pdo.number;
    constexpr auto cob_id = Mapped.pdo.cob_id;
    constexpr auto period = Mapped.pdo.period;
    static_assert(number >= 1 && number <= Opt.tpdo_count,
                  "TPDO index out of range");
    check_cob_id<number, cob_id>();
    tpdo_.template setup<number, cob_id>(
        detail::described_source<Mapped, Provider>(obj),
        period,
        clock_());
  }

  // binding to a temporary would leave the slot with a dangling pointer
  template<auto const& Mapped, auto Provider, typename T>
  void setup_tpdo(T const&&) = delete;

  // Sets the slot of the TPDO `Mapped`, an MPDO in source address mode, up
  // from its description: the `mapped_pdo` of the PDO and the list of its
  // objects that `pdo_of` makes, the constant that the description of the
  // dictionary takes too. The slot is that of the number of the PDO, with its
  // COB-ID and its period. Once a period it sends the frame that `scanner`,
  // the source of the objects of the same list, gives (`mpdo_scanner::next`),
  // and tells it of each frame that `send` takes (`mpdo_scanner::sent`); a
  // period in which it gives none sends nothing, and an MPDO has no counter.
  // The source is a member of the application that outlives the server, as
  // its context does. The program is ill-formed if the PDO is no TPDO or if
  // its number is past the slots or its COB-ID one that `setup_tpdo<I, CobId>`
  // refuses, and the overload is not viable if `scanner` sends another list
  // than the PDO. The number, the COB-ID and the period are read from
  // `Mapped.pdo` in constant expressions only, so that the PDO does not reach
  // the image, and the slot is written whole, as by any `setup_tpdo`.
  template<auto const& Mapped, typename C, auto const& Scans>
    requires(&Scans == &mapped_list<Mapped>)
  void setup_tpdo(mpdo_scanner<C, Scans>& scanner)
  {
    static_assert(Mapped.pdo.direction == pdo_direction::tx,
                  "setup_tpdo takes a PDO that the device sends");
    constexpr auto number = Mapped.pdo.number;
    constexpr auto cob_id = Mapped.pdo.cob_id;
    constexpr auto period = Mapped.pdo.period;
    static_assert(number >= 1 && number <= Opt.tpdo_count,
                  "TPDO index out of range");
    check_cob_id<number, cob_id>();
    tpdo_.template setup<number, cob_id>(detail::scanned_source(scanner),
                                         period,
                                         clock_());
  }

  // ---- heartbeat / sync / nmt ----

  void set_heartbeat_period(std::chrono::milliseconds period)
  {
    auto now = clock_();
    hb_producer_.set_period(period, now);
  }

  void set_sync_period(std::chrono::milliseconds period)
  {
    auto now = clock_();
    sync_producer_.set_period(period, now);
  }

  void on_nmt_change(emb::delegate<void(nmt_state)> handler)
  {
    on_nmt_change_ = handler;
  }

  void on_reset_node(emb::delegate<void()> handler)
  {
    on_reset_node_ = handler;
  }

  void on_reset_communication(emb::delegate<void()> handler)
  {
    on_reset_communication_ = handler;
  }

  bool watch_heartbeat(node_id remote,
                       std::chrono::milliseconds timeout,
                       emb::delegate<void(node_id)> on_lost)
  {
    auto now = clock_();
    return hb_consumer_.watch(remote, timeout, on_lost, now);
  }

  // ---- emcy ----

  bool emit_emcy(std::uint16_t error_code,
                 std::uint8_t error_register,
                 std::array<std::uint8_t, 5> manufacturer = {})
  {
    return emcy_.emit(error_code, error_register, manufacturer);
  }

private:
  // Checks the COB-ID `CobId` of PDO `I` at compile time: only PDOs 1 to 4
  // have the predefined COB-ID, and a custom one is of 11 bits and not
  // restricted (`is_restricted_cob_id`), as it would take the COB-ID of a
  // service of the node or one that CiA 301 reserves.
  template<std::size_t I, pdo_id CobId>
  static consteval void check_cob_id()
  {
    static_assert(CobId.is_custom || I <= 4,
                  "PDO >= 5 requires pdo_id::custom(...)");
    static_assert(!CobId.is_custom
                      || (CobId.value >= 1 && CobId.value <= 0x7FF),
                  "COB-ID out of 11-bit range");
    static_assert(!CobId.is_custom || !is_restricted_cob_id(CobId.value),
                  "COB-ID restricted (is_restricted_cob_id)");
  }

  void enqueue_rx(frame_t const& frame)
  {
    (void)rx_queue_.try_push(frame); // drop-newest on full
  }

  void dispatch_rx(frame_t const& frame)
  {
    if (nmt_.match(frame)) {
      handle_nmt_command(frame);
      return;
    }
    auto const sdo = sdo_.try_handle(frame);
    if (sdo) return;
    switch (sdo.error()) {
    case detail::sdo_refusal::unmatched:
      break;
    case detail::sdo_refusal::length_error:
      return;
    }
    auto now = clock_();
    auto const rpdo = rpdo_.try_handle(frame, now, nmt_.state());
    if (rpdo) return;
    switch (rpdo.error()) {
    case detail::rpdo_refusal::unmatched:
      break;
    case detail::rpdo_refusal::length_error:
      emcy_.emit(emcy_code::pdo_length_error,
                 error_register::generic | error_register::communication);
      return;
    case detail::rpdo_refusal::not_operational:
    case detail::rpdo_refusal::repeated_length_error:
      return;
    }
    hb_consumer_.try_handle(frame, now);
  }

  void apply_nmt_state(nmt_state s)
  {
    if (s == nmt_.state()) return;
    nmt_.set_state(s);
    auto now = clock_();
    if (s == nmt_state::operational) rpdo_.reset_timers(now);
    if (on_nmt_change_) on_nmt_change_(s);
  }

  void handle_nmt_command(frame_t const& frame)
  {
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

  emb::spsc_queue<frame_t, Opt.rx_queue_capacity> rx_queue_;

  detail::nmt_slave nmt_;
  detail::hb_producer hb_producer_;
  detail::sync_producer sync_producer_;
  detail::hb_consumer hb_consumer_;
  detail::emcy_producer emcy_;
  detail::sdo_server<Ctx> sdo_;
  detail::tpdo_producer<Opt.tpdo_count> tpdo_;
  detail::rpdo_consumer<Opt.rpdo_count> rpdo_;

  emb::delegate<void(nmt_state)> on_nmt_change_;
  emb::delegate<void()> on_reset_node_;
  emb::delegate<void()> on_reset_communication_;
};

} // namespace canopen
} // namespace can
} // namespace emb
