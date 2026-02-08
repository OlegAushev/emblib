#include <emb/chrono.hpp>

namespace emb {
namespace chrono {

__attribute__((weak)) std::chrono::time_point<steady_clock>
steady_clock::now() {
  return time_point{};
}

} // namespace chrono
} // namespace emb
