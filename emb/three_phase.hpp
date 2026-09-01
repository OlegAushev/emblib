#pragma once

namespace emb {

// One value per phase of a three-phase system, named a/b/c. This is the
// shape of anything addressed by phase name -- per-phase readings, duty
// cycles -- as opposed to legs, channels and pins, which stay indexed.
template<typename T>
struct three_phase {
  T a;
  T b;
  T c;
};

} // namespace emb
