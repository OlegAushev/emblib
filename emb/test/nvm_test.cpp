#include <array>
#include <bit>
#include <cassert>
#include <cstring>

#include <emb/nvm.hpp>
#include <emb/units.hpp>

namespace {

using namespace emb;

// -- Test backend --

struct backend {
  using addr_type = std::uint32_t;
  static constexpr std::size_t capacity = 512;

  std::array<std::uint8_t, capacity> mem{};

  template<typename T>
  constexpr auto read(addr_type addr) -> std::expected<T, nvm::error> {
    std::array<std::uint8_t, sizeof(T)> buf{};
    for (auto i = 0uz; i < sizeof(T); ++i)
      buf[i] = mem[addr + i];
    return std::bit_cast<T>(buf);
  }

  template<typename T>
  constexpr auto write(addr_type addr, T const& val)
      -> std::expected<void, nvm::error> {
    auto buf = std::bit_cast<std::array<std::uint8_t, sizeof(T)>>(val);
    for (auto i = 0uz; i < sizeof(T); ++i)
      mem[addr + i] = buf[i];
    return {};
  }
};

// -- Layout --

inline constexpr nvm::layout<
    nvm::parameter<"motor.pole_pairs", std::int32_t{1}>,
    nvm::parameter<"motor.R", 0.050f>,
    nvm::parameter<"gains", std::array{100.f, 200.f, 300.f}>,
    nvm::parameter<"angle.offset", units::deg_f32{15.0f}>>
    nvm_layout{.base = 0};

using registry_type = nvm::registry<backend, nvm_layout>;

static_assert(
    std::same_as<registry_type::value_type<"motor.pole_pairs">, std::int32_t>
);
static_assert(std::same_as<registry_type::value_type<"motor.R">, float>);
static_assert(
    std::same_as<registry_type::value_type<"gains">, std::array<float, 3>>
);
static_assert(
    std::same_as<registry_type::value_type<"angle.offset">, units::deg_f32>
);

// -- Tests --

static constexpr std::size_t frame_overhead = sizeof(nvm::fnv1a_32::type)
                                            + sizeof(nvm::crc32::type);

static constexpr bool test_round_trip() {
  backend storage{};
  registry_type settings{storage};

  // size includes hash+crc overhead per entry
  assert(
      settings.size
      == 4 * frame_overhead
             + sizeof(std::int32_t)
             + sizeof(float)
             + 3 * sizeof(float)
             + sizeof(units::deg_f32)
  );

  auto r1 = settings.set<"motor.R">(0.42f);
  assert(r1.has_value());
  auto r2 = settings.set<"motor.pole_pairs">(2);
  assert(r2.has_value());
  auto r3 = settings.set<"gains">({1.f, 2.f, 3.f});
  assert(r3.has_value());
  auto r4 = settings.set<"angle.offset">(units::deg_f32{30.0f});
  assert(r4.has_value());

  auto motor_R = settings.get<"motor.R">();
  assert(motor_R.has_value() && *motor_R == 0.42f);

  auto motor_p = settings.get<"motor.pole_pairs">();
  assert(motor_p.has_value() && *motor_p == 2);

  auto gains = settings.get<"gains">();
  assert(gains.has_value());
  assert((*gains)[0] == 1.f && (*gains)[1] == 2.f && (*gains)[2] == 3.f);

  auto angle = settings.get<"angle.offset">();
  assert(angle.has_value() && *angle == units::deg_f32{30.0f});

  return true;
}

static constexpr bool test_hash_mismatch() {
  backend storage{};
  registry_type settings{storage};

  [[maybe_unused]] auto r = settings.set<"motor.pole_pairs">(10);

  auto motor_p = settings.get<"motor.pole_pairs">();
  assert(motor_p.has_value() && *motor_p == 10);

  // corrupt the hash byte of motor.pole_pairs frame
  constexpr auto off = nvm_layout.offset_of<"motor.pole_pairs">();
  storage.mem[off] ^= 0xFF;

  motor_p = settings.get<"motor.pole_pairs">();
  assert(!motor_p.has_value());
  assert(motor_p.error() == nvm::error::hash_mismatch);

  return true;
}

static constexpr bool test_crc_mismatch() {
  backend storage{};
  registry_type settings{storage};

  [[maybe_unused]] auto r = settings.set<"motor.R">(0.42f);

  auto motor_R = settings.get<"motor.R">();
  assert(motor_R.has_value() && *motor_R == 0.42f);

  // corrupt a value byte (hash stays valid, CRC will mismatch)
  constexpr auto off = nvm_layout.offset_of<"motor.R">();
  storage.mem[off + sizeof(nvm::fnv1a_32::type)] ^=
      0xFF; // corrupt first byte of value

  motor_R = settings.get<"motor.R">();
  assert(!motor_R.has_value());
  assert(motor_R.error() == nvm::error::crc_mismatch);

  return true;
}

static constexpr bool test_reset() {
  backend storage{};
  registry_type settings{storage};

  [[maybe_unused]] auto r = settings.set<"motor.pole_pairs">(42);

  auto motor_p = settings.get<"motor.pole_pairs">();
  assert(motor_p.has_value() && *motor_p == 42);

  r = settings.reset<"motor.pole_pairs">();

  motor_p = settings.get<"motor.pole_pairs">();
  assert(motor_p.has_value() && *motor_p == 1); // default is 1

  return true;
}

static constexpr bool test_reset_all() {
  backend storage{};
  registry_type settings{storage};

  // fresh storage: nothing readable yet
  assert(!settings.get<"motor.R">().has_value());

  [[maybe_unused]] auto r1 = settings.set<"motor.pole_pairs">(42);
  [[maybe_unused]] auto r2 = settings.set<"motor.R">(0.42f);
  [[maybe_unused]] auto r3 = settings.set<"gains">({1.f, 2.f, 3.f});
  [[maybe_unused]] auto r4 = settings.set<"angle.offset">(
      units::deg_f32{30.0f}
  );

  auto r = settings.reset_all();
  assert(r.has_value());

  auto motor_p = settings.get<"motor.pole_pairs">();
  assert(motor_p.has_value() && *motor_p == 1);

  auto motor_R = settings.get<"motor.R">();
  assert(motor_R.has_value() && *motor_R == 0.050f);

  auto gains = settings.get<"gains">();
  assert(gains.has_value());
  assert((*gains)[0] == 100.f && (*gains)[1] == 200.f && (*gains)[2] == 300.f);

  auto angle = settings.get<"angle.offset">();
  assert(angle.has_value() && *angle == units::deg_f32{15.0f});

  return true;
}

static_assert(test_round_trip());
static_assert(test_hash_mismatch());
static_assert(test_crc_mismatch());
static_assert(test_reset());
static_assert(test_reset_all());

} // namespace
