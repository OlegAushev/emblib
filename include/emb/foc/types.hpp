#pragma once

namespace emb {
namespace foc {

struct vec_polar {
  float mag;
  float theta;
};

struct vec_ab {
  float alpha;
  float beta;
};

struct vec_dq {
  float d;
  float q;
};

} // namespace foc
} // namespace emb
