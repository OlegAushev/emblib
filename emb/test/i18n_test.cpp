#include <emb/i18n.hpp>

#include <iterator>

namespace {

using emb::i18n::is_complete;
using emb::i18n::languages;
using emb::i18n::text;

// The table names each language by its code and its name in English and
// gives its member, in the order of the members.
static_assert(std::size(languages) == 2);
static_assert(languages[0].code == "en" && languages[0].name == "English");
static_assert(languages[0].member == &text::en);
static_assert(languages[1].code == "ru" && languages[1].name == "Russian");
static_assert(languages[1].member == &text::ru);

// A text is complete if it has a text in each language.
static_assert(is_complete({"Voltage", "Напряжение"}));
static_assert(!is_complete({"Voltage", ""}));
static_assert(!is_complete({"", "Напряжение"}));
static_assert(!is_complete({}));

// A text is empty if it is empty in each language, as `text{}` is; a text
// empty in only some of them is neither empty nor complete.
static_assert(text{}.empty());
static_assert(text{"", ""}.empty());
static_assert(!text{"Voltage", ""}.empty());
static_assert(!text{"", "Напряжение"}.empty());
static_assert(!text{"Voltage", "Напряжение"}.empty());

} // namespace
