#pragma once

#include <string_view>

namespace emb::i18n {

// The struct `text` holds a text for people in each language a device shows
// it in: `en`, in English, and `ru`, in Russian, e.g. the label of a
// parameter or of a status. A text that only constant evaluation reads
// stays out of the image. A language added later becomes a member with a
// default, so that `{"…", "…"}` written before stays valid.
struct text {
  std::string_view en;
  std::string_view ru;

  friend constexpr bool operator==(text const&, text const&) = default;
};

} // namespace emb::i18n
