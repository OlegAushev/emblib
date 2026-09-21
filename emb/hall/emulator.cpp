#include <emb/hall/emulator.hpp>

namespace emb::hall {

sector emulated_sector(emb::units::erad_f32 angle, sector current)
{
  return detail::sector_at(angle, current);
}

} // namespace emb::hall
