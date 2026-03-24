#include <array>
#include <bit>
#include <cstring>
#include <emb/nvm.hpp>

// -- Parameter definitions --

template<>
struct emb::nvm::parameter<"motor.pole_pairs">
    : emb::nvm::parameter_traits<"motor.pole_pairs", int32_t, 1> {};

template<>
struct emb::nvm::parameter<"motor.R">
    : emb::nvm::parameter_traits<"motor.R", float, 0.050f> {};

template<>
struct emb::nvm::parameter<"gains"> : emb::nvm::parameter_traits<
                                          "gains",
                                          std::array<float, 3>,
                                          {100.f, 200.f, 300.f}> {};

namespace emb {
namespace internal {
namespace tests {

// -- Test backend --

struct backend {
  using addr_type = uint32_t;
  static constexpr size_t capacity = 512;

  std::array<uint8_t, capacity> mem{};

  template<typename T>
  constexpr auto read(addr_type addr) -> std::expected<T, nvm::error> {
    std::array<uint8_t, sizeof(T)> buf{};
    for (size_t i = 0; i < sizeof(T); ++i)
      buf[i] = mem[addr + i];
    return std::bit_cast<T>(buf);
  }

  template<typename T>
  constexpr auto write(addr_type addr, T const& val)
      -> std::expected<void, nvm::error> {
    auto buf = std::bit_cast<std::array<uint8_t, sizeof(T)>>(val);
    for (size_t i = 0; i < sizeof(T); ++i)
      mem[addr + i] = buf[i];
    return {};
  }
};

// -- Layout --

inline constexpr nvm::layout nvm_layout{
    .base = 0,
    .entries = {
        nvm::parameter<"motor.pole_pairs">::entry,
        nvm::parameter<"motor.R">::entry,
        nvm::parameter<"gains">::entry
    }
};

// -- Tests --

static constexpr size_t frame_overhead = sizeof(nvm::fnv1a_32::type)
                                       + sizeof(nvm::crc32::type);

static constexpr bool test_round_trip() {
  backend storage{};
  nvm::registry<backend, nvm_layout> settings{storage};

  // size includes hash+crc overhead per entry
  assert(
      settings.size
      == 3 * frame_overhead
             + sizeof(int32_t)
             + sizeof(float)
             + 3 * sizeof(float)
  );

  auto r1 = settings.set<"motor.R">(0.42f);
  assert(r1.has_value());
  auto r2 = settings.set<"motor.pole_pairs">(2);
  assert(r2.has_value());
  auto r3 = settings.set<"gains">({1.f, 2.f, 3.f});
  assert(r3.has_value());

  auto motor_R = settings.get<"motor.R">();
  assert(motor_R.has_value() && *motor_R == 0.42f);

  auto motor_p = settings.get<"motor.pole_pairs">();
  assert(motor_p.has_value() && *motor_p == 2);

  auto gains = settings.get<"gains">();
  assert(gains.has_value());
  assert((*gains)[0] == 1.f && (*gains)[1] == 2.f && (*gains)[2] == 3.f);

  return true;
}

static constexpr bool test_hash_mismatch() {
  backend storage{};
  nvm::registry<backend, nvm_layout> settings{storage};

  [[maybe_unused]] auto r = settings.set<"motor.pole_pairs">(10);

  auto motor_p = settings.get<"motor.pole_pairs">();
  assert(motor_p.has_value() && *motor_p == 10);

  // corrupt the hash byte of motor.pole_pairs frame
  constexpr auto off = nvm_layout.offset_of(
      nvm::parameter<"motor.pole_pairs">::hash
  );
  storage.mem[off] ^= 0xFF;

  motor_p = settings.get<"motor.pole_pairs">();
  assert(!motor_p.has_value());
  assert(motor_p.error() == nvm::error::hash_mismatch);

  return true;
}

static constexpr bool test_crc_mismatch() {
  backend storage{};
  nvm::registry<backend, nvm_layout> settings{storage};

  [[maybe_unused]] auto r = settings.set<"motor.R">(0.42f);

  auto motor_R = settings.get<"motor.R">();
  assert(motor_R.has_value() && *motor_R == 0.42f);

  // corrupt a value byte (hash stays valid, CRC will mismatch)
  constexpr auto off = nvm_layout.offset_of(nvm::parameter<"motor.R">::hash);
  storage.mem[off + sizeof(nvm::parameter<"motor.R">::hash_type)] ^=
      0xFF; // corrupt first byte of value

  motor_R = settings.get<"motor.R">();
  assert(!motor_R.has_value());
  assert(motor_R.error() == nvm::error::crc_mismatch);

  return true;
}

static constexpr bool test_reset() {
  backend storage{};
  nvm::registry<backend, nvm_layout> settings{storage};

  [[maybe_unused]] auto r = settings.set<"motor.pole_pairs">(42);

  auto motor_p = settings.get<"motor.pole_pairs">();
  assert(motor_p.has_value() && *motor_p == 42);

  r = settings.reset<"motor.pole_pairs">();

  motor_p = settings.get<"motor.pole_pairs">();
  assert(motor_p.has_value() && *motor_p == 1); // default is 1

  return true;
}

static_assert(test_round_trip());
static_assert(test_hash_mismatch());
static_assert(test_crc_mismatch());
static_assert(test_reset());

} // namespace tests
} // namespace internal
} // namespace emb
