#include <cstdint>

#include <emb/can/canopen/od_dictionary.hpp>
#include <emb/can/canopen/od_handlers.hpp>
#include <emb/can/canopen/od_section.hpp>
#include <emb/settings/section.hpp>
#include <emb/test/mock/ram_storage.hpp>

namespace {

using namespace emb::can::canopen;
using enum od_value_type;

namespace settings = emb::settings;

struct ctx {};

inline constexpr auto schema = settings::make_schema(
    settings::param("drive.phase_swap", false),
    settings::param("motor.p",
                    std::int32_t{11},
                    {.min = std::int32_t{1}, .max = std::int32_t{64}}),
    settings::param("prod.serial", std::uint32_t{0}, {.writable = false}));

using flash = emb::test::ram_storage<1024, 4, true, 128>;
inline constexpr settings::placement flash_placement{.magic = 0x47464354u,
                                                     .base = 0,
                                                     .slot_capacity = 64,
                                                     .slot_count = 4,
                                                     .slots_per_block = 2};

constinit settings::section<schema, flash, flash_placement> config;

using bridge = od_settings_for<config>;
using status = od_section_status<config>;

template<auto Binding>
consteval od_binding<ctx> bind()
{
  return Binding;
}

static_assert(bridge::catalog.names.size() == schema.count);
static_assert(bind<bridge::rw<"motor.p">>().type == int32);
static_assert(bind<bridge::rw<"motor.p">>().arg == 1);
static_assert(bind<bridge::ro<"prod.serial">>().write == nullptr);

static_assert(status::magic() == flash_placement.magic);
static_assert(status::slot_count() == 4);
static_assert(status::slot_capacity() == 64);
static_assert(status::slots_per_block() == 2);

consteval bool test_dictionary()
{
  // clang-format off
  static constexpr od_row<ctx> rows[] = {
      {{0x3000, 0x01}, "config", "nvm",   "magic",            "", uint32,  od_ro<status::magic>},
      {{0x3000, 0x02}, "config", "nvm",   "slot_count",       "", uint32,  od_ro<status::slot_count>},
      {{0x3000, 0x03}, "config", "nvm",   "slot_capacity",    "", uint32,  od_ro<status::slot_capacity>},
      {{0x3000, 0x04}, "config", "nvm",   "slots_per_block",  "", uint32,  od_ro<status::slots_per_block>},
      {{0x3000, 0x05}, "config", "nvm",   "sequence",         "", uint32,  od_ro<status::sequence>},
      {{0x3000, 0x06}, "config", "nvm",   "erase_cycles",     "", uint32,  od_ro<status::erase_cycles>},
      {{0x3000, 0x07}, "config", "nvm",   "loaded_slot",      "", uint32,  od_ro<status::loaded_slot>},
      {{0x3000, 0x08}, "config", "nvm",   "loaded_sequence",  "", uint32,  od_ro<status::loaded_sequence>},
      {{0x3000, 0x09}, "config", "nvm",   "valid",            "", boolean, od_ro<status::valid>},
      {{0x3000, 0x0A}, "config", "nvm",   "schema_matched",   "", boolean, od_ro<status::schema_matched>},
      {{0x3000, 0x0B}, "config", "nvm",   "read_failed",      "", boolean, od_ro<status::read_failed>},
      {{0x3000, 0x0C}, "config", "nvm",   "stored",           "", uint32,  od_ro<status::stored>},
      {{0x3000, 0x0D}, "config", "nvm",   "loaded",           "", uint32,  od_ro<status::loaded>},
      {{0x3000, 0x0E}, "config", "nvm",   "unknown",          "", uint32,  od_ro<status::unknown>},
      {{0x3000, 0x0F}, "config", "nvm",   "rejected",         "", uint32,  od_ro<status::rejected>},
      {{0x3000, 0x10}, "config", "nvm",   "missing",          "", uint32,  od_ro<status::missing>},
      {{0x3000, 0x11}, "config", "nvm",   "restart_required", "", boolean, od_ro<status::restart_required>},
      {{0x3000, 0x12}, "config", "nvm",   "changes_pending",  "", boolean, od_ro<status::changes_pending>},
      {{0x3002, 0x01}, "config", "drive", "phase_swap",       "", boolean, bridge::rw<"drive.phase_swap">},
      {{0x3004, 0x01}, "config", "motor", "pole_pairs",       "", int32,   bridge::rw<"motor.p">},
      {{0x3009, 0x01}, "config", "prod",  "serial",           "", uint32,  bridge::ro<"prod.serial">},
  };
  // clang-format on

  auto const dictionary = make_dictionary<rows>();
  od_view<ctx> const view{dictionary};

  ctx c;
  auto const* magic = view.find({0x3000, 0x01});
  if (magic == nullptr) return false;
  return magic->read(c, magic->arg) == od_read_result{flash_placement.magic};
}

static_assert(test_dictionary());

} // namespace
