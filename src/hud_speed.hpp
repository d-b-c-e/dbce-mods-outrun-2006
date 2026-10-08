#pragma once
#include <cmath>

namespace OutRunHudSpeed {
// Supported executable's +0x1F8 is the HUD's km/h base before the optional mph
// multiplier. This is game-display speed, not position-derived physical motion.
// A failed observation must not become a confidently measured zero.
inline bool TryMetresPerSecond(float hudBaseKmh, float& result) noexcept {
    if (!std::isfinite(hudBaseKmh) || hudBaseKmh < 0.0f) return false;
    result = hudBaseKmh / 3.6f;
    return true;
}
}
