#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <type_traits>
#include <utility>

#include "od.hpp"
#include "od_dictionary.hpp"
#include "sdo.hpp"
#include "types.hpp"

#include <emb/can.hpp>
#include <emb/panic.hpp>

namespace emb {
namespace can {
namespace canopen {

namespace detail {

// The number of objects that the list `Scans` names: the subindices of all
// its blocks.
template<auto const& Scans>
inline constexpr std::size_t mpdo_object_count = [] {
  auto count = 0uz;
  for (auto const& scan : Scans) {
    count += scan.count;
  }
  return count;
}();

// The position, among the objects of the list `Scans`, of the first object of
// each of its blocks.
template<auto const& Scans>
inline constexpr auto mpdo_block_firsts = [] {
  std::array<std::size_t, std::size(Scans)> firsts{};
  auto first = 0uz;
  for (auto b = 0uz; b < std::size(Scans); ++b) {
    firsts[b] = first;
    first += Scans[b].count;
  }
  return firsts;
}();

} // namespace detail

// The class template `mpdo_scanner` is the source of an MPDO in source address
// mode (CiA 301 §7.2.3.4.1) that sends the objects of the list `Scans`, an
// object a frame, as the dictionary of the context `Ctx` serves them: the
// slot of a TPDO that the server sets up from the description of the MPDO
// (`server::setup_tpdo`) asks it for a frame once a period (`next`) and tells
// it when `send` took the frame (`sent`). A frame carries the node-ID of the
// node, the index and the subindex of the object and its value, as the data
// of an expedited SDO carries it (`sam_mpdo`).
//
// Which object a frame carries, the rule of selection, is the business of two
// cursors and a count of frames. The cursor of changes searches round, from
// the object after the last one it sent to that object itself, for the first
// object whose value differs from its shadow, the value that `send` last took
// of it; it starts on the last object of the list, so that its first search
// starts with the first. The cursor of the round goes round the list from the
// first object, and changes do not reset it. Every fourth frame that `send`
// took, the one at which the count of frames is 0 modulo 4, comes from the
// round, and so does any frame while no value has changed; the others come
// from the cursor of changes. A frame of the round carries the value of its
// object as it is, changed or not, and becomes its shadow, so that the cursor
// of changes no longer sends it. So, while `send` takes the frames, a change
// alone goes in the first or the second frame after it, k changes at once
// within 4k/3 frames rounded up, and each object of N at least once in 4N
// frames, however often values change. The shadow starts at zeros, so that
// the objects whose values are not 0 go first, as changes.
//
// The shadow, the cursors and the count move only on `sent`: while the
// transmit queue of the driver is full, `send` refuses the frames, and the
// source does not count as sent what never went into the queue. Once `send`
// takes frames again, the source goes on from where it stood, and what changed
// meanwhile goes by the rule of selection. An object whose reader fails is not
// sent and does not count as changed: the round passes on to the next one that
// reads, and with no object that reads, there is no frame that period.
//
// The source finds the objects of the list in the dictionary once, when it is
// constructed, and keeps the reader and the argument of each, its shadow of 4
// bytes, the two cursors, the count and the object, the value and the cursor
// of the last frame that `next` gave; the indices and subindices of the frames
// are constants of the list. An object of the list that the dictionary does
// not serve, or does not let read, makes it call `emb::panic`: the rules of
// the description of the MPDO (`detail::od_check`) refuse such a list, and the
// source is constructed when the device starts. Like the server, it keeps a
// reference to the context, which must outlive it, and its slot keeps its
// address, so it is neither copied nor moved.
template<typename Ctx, auto const& Scans>
  requires some_mpdo_scans<std::remove_cvref_t<decltype(Scans)>>
class mpdo_scanner {
public:
  // Finds the objects of the list in `dictionary`, which the source reads in
  // the context `ctx`, and sends them as the producer `node`; calls
  // `emb::panic` if `dictionary` does not serve one of them or does not let
  // it be read.
  constexpr mpdo_scanner(node_id node, od_view<Ctx> dictionary, Ctx& ctx)
      : node_(node), ctx_(ctx)
  {
    for (auto k = 0uz; k < size; ++k) {
      auto const object = dictionary.find(key_of(k));
      if (!object || object->read == nullptr) emb::panic();
      readers_[k] = object->read;
      args_[k] = object->arg;
    }
  }

  mpdo_scanner(node_id node, od_view<Ctx> dictionary, Ctx&& ctx) = delete;

  mpdo_scanner(mpdo_scanner const&) = delete;
  mpdo_scanner& operator=(mpdo_scanner const&) = delete;

  // Returns the frame to send: that of the first object whose value differs
  // from its shadow, searching round from the one after the last object that
  // the cursor of changes sent to that object itself; but every fourth frame
  // that `send` took, and whenever no value differs, that of the first object
  // of the round that reads. Returns `std::nullopt` if no object reads.
  // Changes nothing but the frame that `sent` would commit.
  constexpr std::optional<payload_t> next()
  {
    candidate_.reset();
    if (frames_ != 0) {
      for (auto step = 1uz; step <= size; ++step) {
        auto const k = static_cast<position>((changes_ + step) % size);
        auto const value = read(k);
        if (value && *value != shadow_[k]) {
          return offer(k, *value, cursor::changes);
        }
      }
    }
    for (auto step = 0uz; step < size; ++step) {
      auto const k = static_cast<position>((round_ + step) % size);
      if (auto const value = read(k)) return offer(k, *value, cursor::round);
    }
    return std::nullopt;
  }

  // Commits the frame of the last `next`, which `send` took: its value becomes
  // the shadow of its object, the cursor that gave it moves on, the cursor of
  // changes to its object and the cursor of the round past it, and the count
  // of frames steps. Does nothing if `next` gave no frame since the last
  // `sent`.
  constexpr void sent()
  {
    if (!candidate_) return;
    auto const& frame = *candidate_;
    shadow_[frame.object] = frame.value;
    if (frame.from == cursor::changes) {
      changes_ = frame.object;
    }
    else {
      round_ = static_cast<position>((frame.object + 1u) % size);
    }
    frames_ = static_cast<std::uint8_t>((frames_ + 1u) % 4u);
    candidate_.reset();
  }

private:
  static constexpr std::size_t size = detail::mpdo_object_count<Scans>;

  static_assert(size >= 1, "mpdo: an MPDO sends at least one object");
  static_assert(size <= 0xFFFF, "mpdo: an MPDO sends at most 65535 objects");

  // The place of an object in the list.
  using position = std::uint16_t;

  enum class cursor : std::uint8_t {
    changes,
    round
  };

  // A frame that `next` gave: its object, the value it carries and the cursor
  // that chose it.
  struct candidate {
    position object;
    expedited_sdo_data value;
    cursor from;
  };

  // Returns the key of the object at `k`: in the block that holds it, the
  // index of the block and its first subindex plus the place of the object in
  // it. The blocks are constants, so no table of keys is kept.
  static constexpr od_key key_of(std::size_t k)
  {
    od_key key{};
    [&]<std::size_t... B>(std::index_sequence<B...>) {
      (
          [&] {
            constexpr auto scan = Scans[B];
            constexpr auto first = detail::mpdo_block_firsts<Scans>[B];
            if (k >= first && k < first + scan.count) {
              key = {scan.index,
                     static_cast<std::uint8_t>(scan.subindex + (k - first))};
            }
          }(),
          ...);
    }(std::make_index_sequence<std::size(Scans)>{});
    return key;
  }

  // Returns the value of the object at `k` as the data of an expedited SDO,
  // or `std::nullopt` if its reader fails.
  constexpr std::optional<expedited_sdo_data> read(position k) const
  {
    auto const value = readers_[k](ctx_, args_[k]);
    if (!value) return std::nullopt;
    return to_sdo_data(*value);
  }

  // Keeps the frame of the object at `k` with `value`, which the cursor `from`
  // chose, for `sent`, and returns its payload.
  constexpr payload_t offer(position k, expedited_sdo_data value, cursor from)
  {
    candidate_ = candidate{.object = k, .value = value, .from = from};
    auto const key = key_of(k);
    return to_payload(sam_mpdo{.producer = node_,
                               .index = key.index,
                               .subindex = key.subindex,
                               .data = value});
  }

  node_id node_;
  Ctx& ctx_;
  std::array<od_read_fn<Ctx>, size> readers_{};
  std::array<std::uint16_t, size> args_{};
  std::array<expedited_sdo_data, size> shadow_{};
  position changes_ = static_cast<position>(size - 1);
  position round_ = 0;
  std::uint8_t frames_ = 0;
  std::optional<candidate> candidate_{};
};

} // namespace canopen
} // namespace can
} // namespace emb
