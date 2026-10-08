#pragma once
// Include after plugin.hpp. Shared with the existing exact calculation recorder;
// this snapshot does not select a model, normalize a tune or open a device.
#include "force_observation.hpp"
namespace OutRunForceObservation {
inline Configuration ReadConfiguration() {
    return {Settings::FFBLateralDeadzone, Settings::FFBGripLoss, Settings::FFBWallImpact, Settings::FFBRoadTexture,
        Settings::FFBTireSlip, Settings::FFBEngineIdle, Settings::FFBSpringStrength, Settings::FFBDamperStrength,
        Settings::FFBSteeringWeight, Settings::FFBWeightTransfer, Settings::FFBGearShift,
        double(Settings::FFBInvertForce), Settings::FFBGlobalStrength};
}
}
