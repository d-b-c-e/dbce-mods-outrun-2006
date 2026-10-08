#pragma once
// Read-only force inputs, independent of native initialization or calculation.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace OutRunForceObservation {
using Configuration = std::array<double, 13>;
inline constexpr const char* ConfigNames[] = {
    "ffb_lateral_deadzone", "ffb_grip_loss", "ffb_wall_impact", "ffb_road_texture",
    "ffb_tire_slip", "ffb_engine_idle", "ffb_spring_strength", "ffb_damper_strength",
    "ffb_steering_weight", "ffb_weight_transfer", "ffb_gear_shift", "ffb_invert", "ffb_global_strength"
};
struct Sample {
    bool observed = false, outputEnabled = false, legacyRequested = false, periodicRequested = false;
    std::uint32_t flags = 0;
    std::array<float, 8> fields{}; // 1c8, 1cc, 1d0, 1d4, 1dc, 1e0, 264, 268
    std::array<std::uint32_t, 4> surfaceMasks{};
    int loadColiType = 0;
    float roughness = 0;
    unsigned long water = 0;
    Configuration config{};
};
inline constexpr const char* FieldNames[] = {
    "field_1c8", "field_1cc", "field_1d0", "field_1d4", "field_1dc", "field_1e0", "field_264", "field_268"
};
// This is the existing force producer's max-over-four aggregation, shared with
// discovery. The lookup can read stage identity but writes only the local water
// flag; it does not call the vibration producer, initialize native, or send FFB.
template<class Car, class Lookup>
void Surface(const Car* car, float& roughness, unsigned long& water, Lookup lookup) {
    water = 0; roughness = 0;
    for (int i = 0; i < 4; ++i)
        roughness = std::max(roughness, static_cast<float>(lookup(
            car->water_flag_24C[i], static_cast<int>(car->OnRoadPlace_5C.loadColiType_0), &water)));
}
template<class Car, class Lookup>
Sample Read(const Car* car, const Configuration& config, bool enabled, bool legacy, bool periodic, Lookup lookup) {
    Sample s;
    s.observed = true; s.outputEnabled = enabled; s.legacyRequested = legacy; s.periodicRequested = periodic;
    s.flags = car->field_8;
    s.fields = {car->field_1C8, car->field_1CC, car->field_1D0, car->field_1D4,
        car->field_1DC, car->field_1E0, car->field_264, car->field_268};
    for (int i = 0; i < 4; ++i) s.surfaceMasks[i] = car->water_flag_24C[i];
    s.loadColiType = static_cast<int>(car->OnRoadPlace_5C.loadColiType_0);
    Surface(car, s.roughness, s.water, lookup);
    s.config = config;
    return s;
}
inline bool Finite(const Sample& s) {
    if (!s.observed || !std::isfinite(s.roughness)) return false;
    for (float v : s.fields) if (!std::isfinite(v)) return false;
    for (double v : s.config) if (!std::isfinite(v)) return false;
    return true;
}
} // namespace OutRunForceObservation
