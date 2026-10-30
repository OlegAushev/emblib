#pragma once

#include <algorithm>
#include <iterator>
#include <string_view>

namespace emb::i18n {

// The struct `text` holds a text for people in each language a device shows
// it in, one member per language of `languages`: `en`, in English, and `ru`,
// in Russian, e.g. the label of a parameter or of a status. A text that only
// constant evaluation reads stays out of the image. A language added later
// becomes a member with a default and a row of `languages`: a text written
// before still compiles, but it is not complete (`is_complete`) until it is
// translated, and where a complete text is required, e.g. the label of an
// exposed parameter, it is refused.
struct text {
  std::string_view en;
  std::string_view ru;

  // Whether the text is empty in each language, as `text{}` is, e.g. the
  // symbol of the unit of a type that names none. A text empty in only some
  // of the languages is neither empty nor complete (`is_complete`).
  [[nodiscard]] constexpr bool empty() const
  {
    return *this == text{};
  }

  friend constexpr bool operator==(text const&, text const&) = default;
};

// The struct `language` describes a language of `text`: its code (ISO 639-1),
// e.g. "en", its name in English, for diagnostics, e.g. "English", and the
// member of `text` that holds the text in it.
struct language {
  std::string_view code;
  std::string_view name;
  std::string_view text::* member;
};

// The languages of `text`, one row per member, in the order of the members.
inline constexpr language languages[] = {
    {"en", "English", &text::en},
    {"ru", "Russian", &text::ru},
};

// A member of `text` without a row, or two rows of one member, fail here.
static_assert([] {
  for (auto i = 0uz; i < std::size(languages); ++i) {
    for (auto j = 0uz; j < i; ++j) {
      if (languages[i].member == languages[j].member) return false;
    }
  }
  return sizeof(text) == std::size(languages) * sizeof(std::string_view);
}());

// Whether `t` has a text in each language.
constexpr bool is_complete(text const& t)
{
  return std::ranges::none_of(languages, [&](language const& lang) {
    return (t.*lang.member).empty();
  });
}

} // namespace emb::i18n
