// Game-side reads for the read-only tick discovery (tick_discovery.hpp, docs/STAGE-PLAYBACK.md step 1). Called on the
// game thread only: once per real update from ReplaceGameUpdateLoop, around GamePlCar_Ctrl from the DirectInputFFB
// hook, and from the outer-loop finalization. Reads named values; writes no game memory.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <spdlog/spdlog.h>
#include "plugin.hpp"
#include "game_addrs.hpp"
#include "game.hpp"
#include "consumer_lifecycle.hpp"
#include "host_lifecycle_policy.hpp"
#include "tick_discovery.hpp"
#include <cstring>
#include <ctime>

namespace TickDiscovery {
static Controller controller;
static LARGE_INTEGER frequency{}, origin{};
static bool carHook = false, rootResolved = false;

// From DirectInputFFBHook::apply. The Vibration hook on the same address is enabled by its own settings; chain order
// between the two inline hooks is not observed here, only which ones are enabled.
void NoteHooks(bool directInputFfbCarHook, bool vibrationCarHookEnabled) {
    carHook = directInputFfbCarHook;
    controller.hooks = std::string("directInputFfbCarHook:") + (carHook ? "1" : "0") +
        " vibrationCarHookEnabled:" + (vibrationCarHookEnabled ? "1" : "0") + " chainOrder:unobserved";
}

static void Log(const std::string& line) { if (!line.empty()) spdlog::info("{}", line); }

static bool NetworkActive() {
    if (!Game::SumoNet_CurNetDriver || !*Game::SumoNet_CurNetDriver) return false;
    const auto* driver = *Game::SumoNet_CurNetDriver;
    // SumoNet_NetDriver's helpers compare absolute vtable addresses; compare relocated ones here.
    const auto vtable = reinterpret_cast<std::uintptr_t>(Module::exe_ptr(0x627EB8 - 0x400000));
    const auto lan = reinterpret_cast<std::uintptr_t>(Module::exe_ptr(0x627BC8 - 0x400000));
    return driver->vftable == vtable || driver->vftable == lan || driver->is_in_lobby_5;
}

void OnUpdate() {
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
}

static void Copy(std::array<float, 16>& out, const D3DMATRIX& m) { std::memcpy(out.data(), &m, sizeof(float) * 16); }

void Observe(EVWORK_CAR* car, bool post) {
    if (!controller.session || !car) return;
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
    controller.Observe(post ? Phase::Post : Phase::Pre, o);
}

void FinalizeForExit() { Log(controller.Finalize()); }
} // namespace TickDiscovery
