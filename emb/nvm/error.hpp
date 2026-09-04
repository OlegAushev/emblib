#pragma once

namespace emb {
namespace nvm {

// What a non-volatile memory driver can fail with. Kept apart from the
// storage concept, which deliberately lets a backend bring its own error
// type: this is simply the vocabulary the drivers in this library share.
//
// Nothing here is about the meaning of what is stored. Integrity is a
// record's business, and a record reports it as a load result, not as a
// code from a medium that did its job.
enum class error {
  timeout,
  bus_error,
  invalid_argument,
  access_denied,
};

} // namespace nvm
} // namespace emb
