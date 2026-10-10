#pragma once
// POV hat directions as remap button bindings: 128 + hat * 4 + direction (0 up, 1 right, 2 down, 3 left), the encoding
// recomp-ui's raw_hat_binding.h uses. DIJOYSTATE2 has four POVs, so 128..143; 0..127 stay buttons. A direction is
// pressed by the toolkit contract's hat rule (STD-033): the POV is not centred and is within 4500 of the direction,
// so a diagonal presses both neighbours.
#include <cstdlib>

namespace PovBinding
{
constexpr int Base = 128, Last = Base + 4 * 4 - 1;

inline bool IsPov(int binding) { return binding >= Base && binding <= Last; }
inline int Hat(int binding) { return (binding - Base) / 4; }
inline int Direction(int binding) { return (binding - Base) % 4; }

// hat 0..3 and a cardinal angle (0, 9000, 18000, 27000) -> binding; -1 otherwise.
inline int Encode(int hat, int angle)
{
    if (hat < 0 || hat > 3 || angle < 0 || angle % 9000 != 0 || angle > 27000) return -1;
    return Base + hat * 4 + angle / 9000;
}

inline bool Pressed(unsigned long pov, int binding)
{
    if (!IsPov(binding) || (pov & 0xFFFF) == 0xFFFF) return false;
    long d = std::labs(long(pov % 36000) - long(Direction(binding)) * 9000);
    if (d > 18000) d = 36000 - d;
    return d <= 4500;
}

inline const char* DirectionName(int binding)
{
    static const char* const names[] = {"Up", "Right", "Down", "Left"};
    return IsPov(binding) ? names[Direction(binding)] : "";
}
}
