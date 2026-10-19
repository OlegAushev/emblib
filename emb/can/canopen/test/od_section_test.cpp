#include <cstdint>

#include <emb/can/canopen/od_dictionary.hpp>
#include <emb/can/canopen/od_handlers.hpp>
#include <emb/can/canopen/od_section.hpp>
#include <emb/settings/section.hpp>
#include <emb/test/mock/ram_storage.hpp>

namespace {

using namespace emb::can::canopen;
using enum od_data_type;

namespace settings = emb::settings;

inline constexpr auto drive_phase_swap =
    settings::param("drive.phase_swap",
                    false,
                    {.apply = settings::apply_policy::on_command});
inline constexpr auto motor_p =
    settings::param("motor.p",
                    std::int32_t{11},
                    {.min = std::int32_t{1}, .max = std::int32_t{64}});
inline constexpr auto prod_serial =
    settings::param("prod.serial", std::uint32_t{0}, {.writable = false});

inline constexpr auto schema =
    settings::make_schema(drive_phase_swap, motor_p, prod_serial);

using flash = emb::test::ram_storage<1024, 4, true, 128>;
inline constexpr settings::placement flash_placement{.magic = 0x47464354u,
                                                     .base = 0,
                                                     .slot_capacity = 64,
                                                     .slot_count = 4,
                                                     .slots_per_block = 2};

using test_section = settings::section<schema, flash, flash_placement>;

// The context of the dictionary holds the section, so that the readers can
// be followed through in constant evaluation.
struct ctx {
  flash memory;
  test_section section;
};

constexpr test_section& section_of(ctx& c)
{
  return c.section;
}

using status = od_section_status<section_of>;

// Where the records are kept is a fact of the type of the section.
static_assert(status::magic() == flash_placement.magic);
static_assert(status::slot_count() == 4);
static_assert(status::slot_capacity() == 64);
static_assert(status::slots_per_block() == 2);

// An applier that checks nothing and admits every change.
struct anything {
  struct guard {};

  constexpr bool check(settings::image<schema> const&)
  {
    return true;
  }

  constexpr guard lock()
  {
    return {};
  }

  constexpr bool admit(settings::image<schema> const&,
                       settings::image<schema> const&)
  {
    return true;
  }

  constexpr void configure(settings::image<schema> const&,
                           settings::image<schema> const&)
  {
  }
};

// The readers follow the section through a load, a change and a save, and
// a second section through the load of what the first saved.
consteval bool test_the_readers_follow_the_section()
{
  ctx c;
  c.section.load(c.memory);

  // An empty medium restores nothing.
  if (status::valid(c) || status::read_failed(c)) return false;
  if (status::loaded_slot(c) != 0xFFFFFFFFu) return false;
  if (status::sequence(c) != 0 || status::erase_cycles(c) != 0) return false;
  if (status::unsaved(c) || status::unapplied(c)) return false;
  if (status::restart_required(c)) return false;

  // A value applied on command waits to be applied, one applied after a
  // restart waits for the restart, and both to be saved.
  if (!c.section.set<drive_phase_swap>(true)) return false;
  if (!c.section.set<motor_p>(std::int32_t{7})) return false;
  if (!status::unapplied(c) || !status::unsaved(c)) return false;
  if (!status::restart_required(c)) return false;

  anything applier;
  if (!c.section.apply_all(applier)) return false;
  if (status::unapplied(c)) return false;
  if (!c.section.save(applier)) return false;
  if (status::sequence(c) != 1 || status::erase_cycles(c) != 0) return false;
  if (status::unsaved(c) || !status::restart_required(c)) return false;

  ctx d{c.memory, {}};
  d.section.load(d.memory);
  if (!status::valid(d) || !status::schema_matched(d)) return false;
  if (status::loaded_slot(d) != 0 || status::loaded_sequence(d) != 1) {
    return false;
  }
  if (status::stored(d) != 3 || status::loaded(d) != 3) return false;
  if (status::unknown(d) != 0 || status::rejected(d) != 0) return false;
  if (status::missing(d) != 0) return false;
  return !status::restart_required(d);
}

static_assert(test_the_readers_follow_the_section());

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
      {{0x3000, 0x12}, "config", "nvm",   "unapplied",        "", boolean, od_ro<status::unapplied>},
      {{0x3000, 0x13}, "config", "nvm",   "unsaved",          "", boolean, od_ro<status::unsaved>},
  };
  // clang-format on

  auto const dictionary = make_dictionary<rows>();
  od_view<ctx> const view{dictionary};

  ctx c;
  c.section.load(c.memory);
  auto const magic = view.find({0x3000, 0x01});
  if (!magic) return false;
  if (magic->read(c, magic->arg) != od_read_result{flash_placement.magic}) {
    return false;
  }
  auto const slot = view.find({0x3000, 0x07});
  if (!slot) return false;
  return slot->read(c, slot->arg) == od_read_result{0xFFFFFFFFu};
}

static_assert(test_dictionary());

} // namespace
