// Game-side reads for the read-only tick discovery (tick_discovery.hpp, docs/STAGE-PLAYBACK.md step 1). Called on the
// game thread only: once per real update from ReplaceGameUpdateLoop, around GamePlCar_Ctrl from the DirectInputFFB
// hook, and from the outer-loop finalization. Reads named values; writes no game memory.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <spdlog/spdlog.h>
#include "plugin.hpp"
#include "legacy_force_configuration.hpp"
#include "game_addrs.hpp"
#include "game.hpp"
#include "consumer_lifecycle.hpp"
#include "host_lifecycle_policy.hpp"
#include "tick_discovery.hpp"
#include <cstring>
#include <ctime>
#include <cstddef>

extern double __cdecl sub_1149C0(unsigned int surfaceMask, int loadColiType, DWORD* waterFlag);

namespace TickDiscovery {
static Controller controller;
static LARGE_INTEGER frequency{}, origin{};
static bool carHook = false, rootResolved = false, faulted = false;

// From DirectInputFFBHook::apply. The Vibration hook on the same address is enabled by its own settings; chain order
// between the two inline hooks is not observed here, only which ones are enabled.
void NoteHooks(bool directInputFfbCarHook, bool vibrationCarHookEnabled) {
    carHook = directInputFfbCarHook;
    controller.hooks = std::string("directInputFfbCarHook:") + (carHook ? "1" : "0") +
        " vibrationCarHookEnabled:" + (vibrationCarHookEnabled ? "1" : "0") + " chainOrder:unobserved";
}

static void Log(const std::string& line) { try { if (!line.empty()) spdlog::info("{}", line); } catch (...) {} }

// Nothing from discovery may escape into the game's update loop, car tick or exit. A fault stops discovery for the
// rest of the session; the window it held is dropped (its outcome stays as last written, if any).
static void Fault() noexcept
{
    faulted = true;
    try { controller.session.reset(); spdlog::warn("TickDiscovery: stopped after an internal error"); } catch (...) {}
}

static bool NetworkActive() {
    if (!Game::SumoNet_CurNetDriver || !*Game::SumoNet_CurNetDriver) return false;
    const auto* driver = *Game::SumoNet_CurNetDriver;
    // SumoNet_NetDriver's helpers compare absolute vtable addresses; compare relocated ones here.
    const auto vtable = reinterpret_cast<std::uintptr_t>(Module::exe_ptr(0x627EB8 - 0x400000));
    const auto lan = reinterpret_cast<std::uintptr_t>(Module::exe_ptr(0x627BC8 - 0x400000));
    return driver->vftable == vtable || driver->vftable == lan || driver->is_in_lobby_5;
}

void OnUpdate() {
    if (faulted) return;
    try {
    if (!rootResolved) {
        rootResolved = true;
        wchar_t base[MAX_PATH]{};
        const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", base, MAX_PATH);
        if (n && n < MAX_PATH) controller.root = std::wstring(base) + L"\\Dbce\\StagePlayback\\outrun-discovery";
        QueryPerformanceFrequency(&frequency);
    }
    const bool hadSession = controller.session != nullptr;
    const bool inGame = Game::current_mode && *Game::current_mode == STATE_GAME;
    const char* unavailable = !ConsumerLifecycle::hostVerified.load() ? "game build not verified" :
        !carHook ? "car hook inactive" : "";
    Log(controller.OnUpdate(inGame, NetworkActive(), (long long)std::time(nullptr), OutRunLifecycle::ExactDiskSha256, unavailable));
    if (!hadSession && controller.session) QueryPerformanceCounter(&origin);
    } catch (...) { Fault(); }
}

static void Copy(std::array<float, 16>& out, const D3DMATRIX& m) { std::memcpy(out.data(), &m, sizeof(float) * 16); }

// Same exact-build camera global used by FixZBufferPrecision. No new hook or
// camera update is introduced: observe the current values at the car boundary.
static_assert(offsetof(EvWorkCamera, CamFov_AC) == 0xAC);
static_assert(offsetof(EvWorkCamera, cam_pos_F8) == 0xF8);
static_assert(offsetof(EvWorkCamera, d3dmatrix140) == 0x140);
static_assert(offsetof(EvWorkCamera, cam_matrix_1C0) == 0x1C0);
static_assert(offsetof(EvWorkCamera, d3dmatrix2C0) == 0x2C0);
static_assert(offsetof(EvWorkCamera, cam_mode_timer_364) == 0x364);
static_assert(offsetof(EVWORK_CAR, field_1F8) == 0x1F8);
static CameraObservation Camera() {
    const auto* camera = Module::exe_ptr<EvWorkCamera>(0x39FE10);
    CameraObservation c;
    c.observed = true; c.mode = camera->cam_mode_34A;
    c.fov = camera->CamFov_AC; c.znear = camera->perspective_znear_BC;
    c.zfar = camera->perspective_zfar_C0; c.modeTimer = camera->cam_mode_timer_364;
    c.position = { camera->cam_pos_F8.x, camera->cam_pos_F8.y, camera->cam_pos_F8.z };
    c.look = { camera->look_pos_104.x, camera->look_pos_104.y, camera->look_pos_104.z };
    c.angle = { camera->cam_ang_128.x, camera->cam_ang_128.y, camera->cam_ang_128.z };
    Copy(c.matrices[0], camera->d3dmatrix140); Copy(c.matrices[1], camera->d3dmatrix180);
    Copy(c.matrices[2], camera->cam_matrix_1C0); Copy(c.matrices[3], camera->d3dmatrix200);
    Copy(c.matrices[4], camera->d3dmatrix240); Copy(c.matrices[5], camera->d3dmatrix280);
    Copy(c.matrices[6], camera->d3dmatrix2C0);
    return c;
}

void Observe(EVWORK_CAR* car, bool post) {
    if (faulted || !controller.session || !car) return;
    try {
    Observation o;
    LARGE_INTEGER now{}; QueryPerformanceCounter(&now);
    o.micros = frequency.QuadPart ? (now.QuadPart - origin.QuadPart) * 1000000 / frequency.QuadPart : 0;
    o.car = reinterpret_cast<std::uintptr_t>(car);
    o.playerCar = car == Game::pl_car();
    o.network = NetworkActive();
    o.ticks = Game::sprani_num_ticks ? *Game::sprani_num_ticks : -1;
    o.appTime = Game::app_time ? *Game::app_time : -1;
    o.powerOn = Game::power_on_timer ? *Game::power_on_timer : -1;
    o.currentMode = Game::current_mode ? int(*Game::current_mode) : -1;
    o.gameMode = Game::game_mode ? *Game::game_mode : -1;
    o.stage = Game::stg_stage_num ? int(*Game::stg_stage_num) : -1;
    o.carId = car->car_id_10; o.carKind = car->car_kind_11; o.carColour = car->car_color_12;
    o.manual = car->manu_transmission_enable_13;
    o.flags = car->flags_4; o.gear = car->cur_gear_208; o.pedal = car->pedal_amount_34; o.speed = car->field_1C4;
    o.position = { car->position_14.x, car->position_14.y, car->position_14.z };
    o.velocity = { car->spd_mb_20.x, car->spd_mb_20.y, car->spd_mb_20.z };
    Copy(o.m70, car->matrix_70); Copy(o.mB0, car->matrix_B0); Copy(o.mF0, car->matrix_F0);
    o.camera = Camera();
    if (o.playerCar && !o.network) {
        // Exact-build HUD routine 0x4BCF10 consumes this field before its
        // km/h or mph display multiplier. Observe only; do not change telemetry
        // or the legacy force law before matching a live HUD sample.
        o.hudSpeedObserved = true;
        o.hudSpeedBase = car->field_1F8;
        o.force = OutRunForceObservation::Read(car, OutRunForceObservation::ReadConfiguration(),
            Settings::DirectInputFFB, Settings::FFBProfile.empty() || _stricmp(Settings::FFBProfile.c_str(), "legacy") == 0,
            Settings::FFBUsePeriodicEffects, sub_1149C0);
    }
    controller.Observe(post ? Phase::Post : Phase::Pre, o);
    } catch (...) { Fault(); }
}

void FinalizeForExit() { if (faulted) return; try { Log(controller.Finalize()); } catch (...) { Fault(); } }
} // namespace TickDiscovery
