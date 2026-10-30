#include <emb/hall/sector.hpp>
#include <emb/meta/enum_info.hpp>

#include <cstdint>
#include <string_view>

namespace {

using namespace emb::hall;

// Every sector is labelled, in the order of the sectors.
static_assert(emb::enum_with_info<sector>);
static_assert(emb::enum_info_of<sector>().size() == 6);
static_assert(emb::enum_info_of<sector>()[3].value() == sector::bc);
static_assert(emb::enum_info_of<sector>()[3].name() == "bc");

// Returns whether the label of each sector, the same in both languages,
// names the inputs high in it and gives its Hall code, the bits of C, B and
// A from the left, as `hall_code` gives them: "CA (101)" for `sector::ca`.
consteval bool test_the_labels_tell_the_hall_code()
{
  constexpr std::string_view inputs = "ABC";
  for (auto const& enumerator : emb::enum_info_of<sector>()) {
    std::uint32_t const code = hall_code(enumerator.value());
    auto const label = enumerator.label().en;
    if (enumerator.label().ru != label) return false;

    auto const space = label.find(' ');
    if (space == std::string_view::npos || label.size() != space + 6) {
      return false;
    }
    if (label[space + 1] != '(' || label.back() != ')') return false;
    auto const letters = label.substr(0, space);
    for (auto bit = 0uz; bit < 3; ++bit) {
      bool const high = ((code >> bit) & 1u) != 0;
      if (label[space + 4 - bit] != (high ? '1' : '0')) return false;
      bool const named = letters.find(inputs[bit]) != std::string_view::npos;
      if (named != high) return false;
    }
  }
  return true;
}

static_assert(test_the_labels_tell_the_hall_code());

} // namespace
