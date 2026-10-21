#pragma once

#include <string_view>

namespace emb {

// The struct `i18n` holds a text for people in each language a device shows
// it in: `en`, in English, and `ru`, in Russian, e.g. the name of a parameter
// or the description of a fault. A text that only constant evaluation reads
// stays out of the image. A language added later becomes a member with a
// default, so that `{"…", "…"}` written before stays valid.
struct i18n {
  std::string_view en;
  std::string_view ru;

  friend constexpr bool operator==(i18n const&, i18n const&) = default;
};

} // namespace emb
