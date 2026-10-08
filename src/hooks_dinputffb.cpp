// DirectInput Force Feedback via DirectInput COM API
// Provides steering wheel force feedback using EVWORK_CAR telemetry data.
// Effects: steering weight (constant force), collision impact,
//          rumble strip, gear shift, road texture, tire slip.
// Uses IDirectInputEffect::SetParameters with DIEP_START for reliable
// real-time updates on all wheel drivers (SDL3 Haptic doesn't work with Moza/DD wheels).

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <dinput.h>
#include <commctrl.h>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "comctl32.lib")
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <string>
#include <array>
#include <memory>
#include <new>
#include <exception>

#include "hook_mgr.hpp"
#include "plugin.hpp"
#include "game_addrs.hpp"
#include "game.hpp"
#include "telemetry.hpp"
#include "wheelffb.h"      // dbce-wheel-mod-toolkit C ABI (lib/toolkit/include)
#include "force_profile.h" // shared force model + versioned tuning profiles
#include "overlay/overlay.hpp"
#include "wheel_ui_snapshot.hpp"
#include "consumer_lifecycle.hpp"

// External vibration data from hooks_forcefeedback.cpp
extern float VibrationLeftMotor;
extern float VibrationRightMotor;

// Surface-type -> roughness coefficient LUT, decompiled from the game's own
// Xbox vibration code (defined in hooks_forcefeedback.cpp). Returns 0.0 for
// asphalt up to 0.9 for rough surfaces; sets *a3 |= 1 on water surfaces.
extern double __cdecl sub_1149C0(unsigned int surfaceMask, int loadColiType, DWORD* waterFlag);

// Forza "Data Out" telemetry, emitted to localhost:8000 for the Moza Pit
// House display and SimHub. The 311-byte FM7 "Dash" layout and its encoder
// now come from dbce-wheel-mod-toolkit rather than a local struct, so the
// sizes every receiver validates against (232 sled / 311 FM7 / 324 Horizon)
// are pinned in one place with a conformance test. The struct fields are the
// same ones, lower-cased: IsRaceOn -> isRaceOn, Speed -> speed and so on.
#include "forza_packet.h"

// Telemetry: shared memory (SimHub) + Forza UDP (Moza Pit House display)
// Forward declarations from hooks_inputremap.cpp.
// Declared above Telemetry (not just above FFB) because the telemetry packet
// now reads live pedal positions from the remap layer.
namespace DInputRemap
{
	IDirectInputDevice8A* GetPrimaryDevice();
	bool IsPrimaryInitialized();
	bool GetPrimaryDeviceGuid(GUID* out);
	int GetTelemetryAccel();   // 0-255, or -1 if no primary device
	int GetTelemetryBrake();   // 0-255, or -1 if no primary device
}

namespace Telemetry
{
	// Shared memory for SimHub plugin
	static HANDLE hMapFile = nullptr;
	static OutRun2006TelemetryData* pData = nullptr;
	static bool initialized = false;
	static uint32_t packetId = 0;

	// Forza UDP for Moza Pit House wheel display
	static SOCKET udpSocket = INVALID_SOCKET;
	static sockaddr_in udpAddr = {};
	static bool udpInitialized = false;
	static const int FORZA_UDP_PORT = 8000;
	static DWORD lastSendTick = 0;
	static bool lastSendFailed = false;

	// Approximate gear ratios for RPM synthesis (OutRun doesn't expose RPM)
	// These create a believable RPM range on the wheel display
	static const float GearRatios[] = { 0.0f, 3.5f, 2.1f, 1.4f, 1.0f, 0.8f, 0.65f };
	static const float MaxRPM = 8500.0f;
	static const float IdleRPM = 900.0f;
	static const float MaxSpeedMps = 90.0f; // ~324 km/h, OutRun top speed approx

	static bool Init()
	{
		if (!Settings::TelemetryEnabled)
			return false;

		// Init shared memory
		const std::string& name = Settings::TelemetrySharedMemName;
		hMapFile = CreateFileMappingA(
			INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
			sizeof(OutRun2006TelemetryData), name.c_str());

		if (!hMapFile)
		{
			spdlog::error("Telemetry: CreateFileMapping failed (err={})", GetLastError());
			return false;
		}

		pData = static_cast<OutRun2006TelemetryData*>(
			MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(OutRun2006TelemetryData)));

		if (!pData)
		{
			spdlog::error("Telemetry: MapViewOfFile failed (err={})", GetLastError());
			CloseHandle(hMapFile);
			hMapFile = nullptr;
			return false;
		}

		memset(pData, 0, sizeof(OutRun2006TelemetryData));
		pData->version = TELEMETRY_VERSION;
		initialized = true;
		spdlog::info("Telemetry: Shared memory '{}' created ({} bytes)", name, sizeof(OutRun2006TelemetryData));

		// Init Forza UDP socket for Moza Pit House
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0)
		{
			udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (udpSocket != INVALID_SOCKET)
			{
				udpAddr.sin_family = AF_INET;
				udpAddr.sin_port = htons(FORZA_UDP_PORT);
				udpAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
				udpInitialized = true;
				spdlog::info("Telemetry: Forza UDP emitter ready (127.0.0.1:{})", FORZA_UDP_PORT);
			}
			else
			{
				spdlog::warn("Telemetry: Failed to create UDP socket (err={})", WSAGetLastError());
			}
		}

		return true;
	}

	static void Write(EVWORK_CAR* car, bool inGameplay)
	{
		if (!Settings::TelemetryEnabled) return;
		// Write to shared memory (SimHub)
		if (pData)
		{
			pData->packetId = ++packetId;
			pData->speed = car->field_1C4;
			pData->steeringAngle = car->field_1D0;
			pData->lateralG1 = car->field_264;
			pData->lateralG2 = car->field_268;
			pData->impactForce = car->field_178;
			pData->gear = car->cur_gear_208;
			pData->prevGear = car->dword1D8;
			pData->stateFlags = car->field_8;
			pData->carFlags = car->flags_4;
			pData->surfaceType[0] = car->water_flag_24C[0];
			pData->surfaceType[1] = car->water_flag_24C[1];
			pData->surfaceType[2] = car->water_flag_24C[2];
			pData->surfaceType[3] = car->water_flag_24C[3];
			pData->vibrationLeft = VibrationLeftMotor;
			pData->vibrationRight = VibrationRightMotor;
			pData->gameMode = Game::current_mode ? *Game::current_mode : 0;
			pData->isInGameplay = inGameplay ? 1 : 0;
		}

		// Send Forza UDP (Moza Pit House wheel display)
		if (udpInitialized && udpSocket != INVALID_SOCKET)
		{
			dbce::forza::Sled sled = {};
			dbce::forza::Dash dash = {};

			sled.isRaceOn = inGameplay ? 1 : 0;
			sled.timestampMs = GetTickCount();

			// Speed: convert normalized (0-2+) to m/s
			float speedMps = car->field_1C4 * MaxSpeedMps;
			dash.speed = speedMps;

			// Gear
			uint32_t gear = car->cur_gear_208;
			dash.gear = (uint8_t)std::clamp(gear, 0u, 10u);

			// Synthesize RPM from speed and gear
			// OutRun doesn't expose RPM, so we calculate a plausible value
			float gearRatio = (gear > 0 && gear < 7) ? GearRatios[gear] : 1.0f;
			float speedNorm = std::clamp(car->field_1C4 / 2.0f, 0.0f, 1.0f);
			float rpm = IdleRPM + speedNorm * gearRatio * (MaxRPM - IdleRPM);
			rpm = std::clamp(rpm, IdleRPM, MaxRPM);

			sled.currentEngineRpm = rpm;
			sled.engineMaxRpm = MaxRPM;
			sled.engineIdleRpm = IdleRPM;

			// Steering angle mapped to Forza's -127..127 range
			dash.steer = dbce::forza::steer11(car->field_1D0);

			// Lateral acceleration (for display)
			sled.accX = car->field_264 + car->field_268;

			// Throttle and brake, straight from the wheel's own pedals.
			//
			// OutRun's car struct never exposes pedal position -- the game only
			// keeps the resulting speed -- so these come from the input remap
			// layer, which is already reading and normalising both axes every
			// frame for the game itself. That makes them true pedal travel
			// rather than something inferred from acceleration.
			//
			// SimHub surfaces Forza's Accel/Brake bytes as GameData.Throttle
			// and GameData.Brake, which is what drives brake lights and
			// pedal-based ShakeIt effects. -1 means no wheel is bound, in which
			// case the field is left at zero rather than asserting a value.
			int accelPedal = DInputRemap::GetTelemetryAccel();
			int brakePedal = DInputRemap::GetTelemetryBrake();
			if (accelPedal >= 0)
				dash.accel = (uint8_t)accelPedal;
			if (brakePedal >= 0)
				dash.brake = (uint8_t)brakePedal;

			// Surface rumble (for display indicators)
			bool offRoad = car->water_flag_24C[0] > 1 || car->water_flag_24C[1] > 1 ||
			               car->water_flag_24C[2] > 1 || car->water_flag_24C[3] > 1;
			if (offRoad)
			{
				for (int i = 0; i < 4; i++)
					sled.surfaceRumble[i] = 1.0f;
			}

			// FM7 "Dash": 232-byte sled + the 79-byte dash block. build() zeroes
			// the buffer, so a field this game has no source for stays 0.
			uint8_t frame[dbce::forza::FORZA_FM7_DASH_311];
			int n = dbce::forza::build(dbce::forza::FORZA_FM7_DASH_311, sled, dash,
			                           frame, sizeof(frame));
			const int sent = sendto(udpSocket, (const char*)frame, n, 0,
				(sockaddr*)&udpAddr, sizeof(udpAddr));
			lastSendFailed = sent != n;
			if (!lastSendFailed) lastSendTick = GetTickCount();
		}
	}

	static void Shutdown()
	{
		if (pData)
		{
			UnmapViewOfFile(pData);
			pData = nullptr;
		}
		if (hMapFile)
		{
			CloseHandle(hMapFile);
			hMapFile = nullptr;
		}
		if (udpSocket != INVALID_SOCKET)
		{
			closesocket(udpSocket);
			udpSocket = INVALID_SOCKET;
		}
		initialized = false;
		spdlog::info("Telemetry: Shared memory closed");
	}

	void SetEnabled(bool enabled)
	{
        ConsumerLifecycle::Gate::Lease lease(ConsumerLifecycle::Runtime());
        if (!lease) return;
		Settings::TelemetryEnabled = enabled;
		if (!enabled)
		{
			Shutdown();
			udpInitialized = false;
			lastSendTick = 0;
			lastSendFailed = false;
		}
	}

	const char* UiStatus()
	{
        ConsumerLifecycle::Gate::Lease lease(ConsumerLifecycle::Runtime());
        if (!lease) return "Stopped for game exit";
		if (!Settings::TelemetryEnabled) return "Off";
		if (lastSendFailed) return "Unavailable - UDP send failed. See Help for the log.";
		if (lastSendTick && GetTickCount() - lastSendTick < 1000) return "Sending (receiver not confirmed)";
		if (initialized && !udpInitialized) return "Unavailable - UDP initialization failed. See Help for the log.";
		return "Waiting for a live car update";
	}
}

// Forward declaration from Proxy.cpp
extern IDirectInput8A* g_RealDirectInput8;

namespace FFB
{
	// DirectInput FFB state. The device, the effects and their whole lifecycle
	// live in WheelFfb.dll (dbce-wheel-mod-toolkit) now; what stays here is the
	// force model, which is the part that is actually about OutRun.
	static WheelFfbApi ffb = {};
	static bool ffbLoaded = false;
	static bool initialized = false;
	static bool initAttempted = false;
	static std::vector<DeviceChoice> uiDevices;
	static std::string deviceError;

	// Hardware periodic effects (road texture / tire slip). 25-40 Hz content
	// synthesized through 60 Hz constant-force updates loses ~26% to zero-order-
	// hold rolloff, more to wheelbase driver smoothing, and 30-60% to tanh
	// compression when riding on steering load. Hardware periodics render inside
	// the wheelbase at full fidelity regardless of our update rate. Slot ids
	// from the DLL; -1 means the driver offers none and the constant-force
	// synthesis fallback carries the vibration instead.
	static int slotRoadTexture = -1;   // GUID_Sine, surface LUT driven
	static int slotTireSlip = -1;      // GUID_Sine, drift chatter / engine idle
	static bool periodicsActive = false;

	// Panic flag: once set (process exit path), no further DI output is issued
	static bool panicStopped = false;

	// The shared force model, when FFBProfile names one.
	//
	// Settings::FFBProfile = legacy          this file's own model (the default)
	//                      = arcade-outrun@1 the toolkit model, tuned as this game shipped
	//                      = <name>@<n>      any profile in force-profiles.ini
	//
	// The profile file sits beside dinput8.dll and is read at startup, so a tune
	// is swapped by editing a text file and restarting - no rebuild. Put your own
	// in force-profiles.user.ini, which an update never overwrites.
	//
	// What does NOT move into the toolkit is everything above this line: reading
	// the car struct, the game's own surface-roughness LUT, crash detection from
	// the speed window, the drift estimate. Working out what the car is doing is
	// per game; deciding how that should FEEL is not, and that is what the shared
	// model owns.
	static bool useSharedModel = false;
	static dbce::force::Profile sharedProfile;
	static dbce::force::Model*  sharedModel = nullptr;
	static dbce::force::Shaper* sharedShaper = nullptr;
	static uint32_t sharedPrevGear = 0;

	// Watchdog: timestamp of last Update() call for staleness detection
	static DWORD lastUpdateTick = 0;

	// Previous frame state for edge detection
	static uint32_t prevGear = 0;
	static uint32_t prevCollisionFlags = 0;
	static float prevSpeed = 0.0f;

	// Exponential moving average for lateral forces (low-pass filter)
	static float smoothedLateral = 0.0f;

	// Crash detection: accumulate speed loss over a sliding window
	static float speedHistory[8] = {};     // Last 8 frames of speed
	static int speedHistoryIdx = 0;

	// Pre-crash lateral history: the collision response corrupts the lateral
	// signal AT impact, so crash direction is read from ~8 frames earlier
	static float latHistory[16] = {};
	static int latHistoryIdx = 0;

	// Crash impulse state
	static int crashImpulseTimer = 0;      // Frames remaining for crash jolt
	static float crashImpulseForce = 0.0f; // Direction and magnitude of crash jolt

	// Previous constant force level (deadband to prevent micro-oscillations)
	static LONG prevConstantLevel = 0;  // DI range: ±10000
	// Previous structural (post-tanh, pre-vibration) level for slew limiting
	static LONG prevStructLevel = 0;

	// Gear shift timer (frames remaining)
	static int gearShiftTimer = 0;

	// Water splash burst (lake/beach stages at high speed)
	static int splashTimer = 0;
	static float splashAmp = 0.0f;

	// Warmup counter: ramp force scaling from 0 to 1 over first N frames
	static int warmupFrames = 0;
	static const int WARMUP_THRESHOLD = 30; // ~0.5 sec at 60Hz

	// Diagnostic: observed steering-derivative (field_1D4) range since last log
	static float diagSteerRateMin = 0.0f;
	static float diagSteerRateMax = 0.0f;

	// Which EVWORK_CAR field actually carries steering position.
	//
	// field_1D0 has been read as "signed steering position, -1..1" and field_1D4
	// as its derivative, but the 2026-08-11 validation drive disagrees: across
	// 170 samples |1D0| never exceeded 0.011 and sat at or below 0.001 in 92% of
	// them, while 1D4 - tracked as a true min/max, not sampled - reached 0.54. A
	// derivative cannot outrun its own integral by fifty times, so at least one
	// label is wrong, and either way the spring and damper terms of the
	// centre-out model have been running on almost nothing. That is why the
	// lateral term still does all the work the redesign meant to demote.
	//
	// Rather than guess an offset, scan the neighbouring floats: one lap with
	// FFBDiagnosticLog=true prints the real range of each. The steering field is
	// the one that reaches roughly +-1 (or +-full lock in whatever unit), changes
	// sign with the corner, and returns to zero on the straights.
	struct FieldProbe { const char* name; float lo; float hi; };
	static FieldProbe steerProbe[] = {
		{ "1C8", 0.0f, 0.0f },
		{ "1CC", 0.0f, 0.0f },
		{ "1D0", 0.0f, 0.0f },   // read as steering position today
		{ "1D4", 0.0f, 0.0f },   // read as steering rate today
		{ "1DC", 0.0f, 0.0f },
		{ "1E0", 0.0f, 0.0f },
	};

	// ---------- Force output, through the toolkit ----------
	//
	// Everything that used to sit here - open the device exclusively, create the
	// constant force, recreate it and ramp back in when the handle dies, two
	// GUID_Sine periodics with the driver strategy probe, re-acquire after a
	// focus change, zero and release in the right order at exit - is WheelFfb.dll
	// now. This project donated most of those rules to it; keeping a second copy
	// is how this repo and art-of-sim-rally came to fix the same bug on
	// different days.

	// FFBGlobalStrength used to be the DirectInput effect gain (dwGain). The DLL
	// runs its effect at full gain, so the strength is applied to the magnitude
	// instead. Both are the same linear scale, so the feel is unchanged.
	static float StrengthScale()
	{
		return std::clamp(Settings::FFBGlobalStrength, 0.0f, 1.0f);
	}

	static void SetConstantForce(LONG magnitude)
	{
        ConsumerLifecycle::Gate::Lease lease(ConsumerLifecycle::Runtime());
        if (!lease || !ConsumerLifecycle::ReadyForActuator(Game::GameHwnd())) return;
		if (!initialized || !ffbLoaded || panicStopped)
			return;
		magnitude = std::clamp(magnitude, (LONG)-10000, (LONG)10000);
		LONG scaled = (LONG)std::clamp((float)magnitude * StrengthScale(), -10000.0f, 10000.0f);
		// Y is always zero: OutRun steers on one axis. The DLL decides how to
		// encode that for the wheel in front of it - three wheels disagreed
		// about direction versus magnitude sign, and it carries the encoding
		// all three accept.
		ffb.SetDeviceForcesXY(scaled, 0);
		prevConstantLevel = magnitude;
	}

	// Envelope update for a hardware periodic effect. Caller rate-limits to
	// ~15 Hz; the DLL additionally drops an update whose magnitude moved less
	// than 5% and period less than 10%, and recreates a slot whose handle died.
	static void UpdatePeriodic(int slot, float magnitude01, float freqHz)
	{
        ConsumerLifecycle::Gate::Lease lease(ConsumerLifecycle::Runtime());
        if (!lease || !ConsumerLifecycle::ReadyForActuator(Game::GameHwnd())) return;
		if (!initialized || slot < 0 || !ffbLoaded || panicStopped)
			return;
		float mag = std::clamp(magnitude01, 0.0f, 1.0f) * StrengthScale();
		ffb.UpdatePeriodicEffect(slot, (int)(mag * 10000.0f),
			(int)(std::clamp(freqHz, 1.0f, 100.0f) * 1000.0f));
	}

	// Zero all force output without tearing anything down (Alt-Tab, menus, watchdog)
	void ZeroAllForces()
	{
        ConsumerLifecycle::Gate::Lease lease(ConsumerLifecycle::Runtime());
        if (!lease) return;
		if (!initialized || panicStopped)
			return;
		if (prevConstantLevel != 0)
			SetConstantForce(0);
		prevStructLevel = 0;
		if (periodicsActive)
		{
			UpdatePeriodic(slotRoadTexture, 0.0f, 25.0f);
			UpdatePeriodic(slotTireSlip, 0.0f, 40.0f);
		}
	}

	// Deferred initialization -- called from Update() on first game tick,
	// because DirectInput needs a valid HWND.
	static bool LoadApi()
	{
		if (!ConsumerLifecycle::ReadyForActuator(Game::GameHwnd())) return false;
		if (ffbLoaded) return true;

		// Loaded at runtime from beside this DLL, never imported: a missing
		// WheelFfb.dll has to disable force feedback, not stop the game from
		// starting, and a static import would do the latter.
		if (!WheelFfb_LoadBeside(&ffb, Module::DllHandle, L"WheelFfb.dll"))
		{
			const char* missing = WheelFfb_MissingExport(&ffb);
			spdlog::error("FFB: WheelFfb.dll unusable ({}) -- force feedback disabled. "
				"Copy WheelFfb.dll next to dinput8.dll.", missing ? missing : "?");
			WheelFfb_Unload(&ffb);
			return false;
		}
        // Establish process lifetime outside loader lock, before any ABI call.
        HMODULE pinned = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(ffb.module), &pinned)) {
            WheelFfb_Unload(&ffb); // No acquisition or native callback has occurred.
            return false;
        }
        ffbLoaded = true;

		// Keep the DLL's own log beside the game, with everything else worth
		// reading after a bad session. Set DBCE_FFB_LOG=0 to silence it.
		{
			char logPath[MAX_PATH] = {};
			if (GetModuleFileNameA(Module::DllHandle, logPath, MAX_PATH))
			{
				char* slash = strrchr(logPath, '\\');
				if (slash)
				{
					strncpy_s(slash + 1, sizeof(logPath) - (slash + 1 - logPath), "OutRun2006Tweaks.ffb.log", _TRUNCATE);
					ffb.SetLogPath(logPath);
				}
			}
		}

		spdlog::info("FFB: WheelFfb.dll loaded (version {})", ffb.GetWheelFfbVersion());
		ffb.SetStrictDeviceSelection(1);
		return true;
	}

	static bool ParseDeviceGuid(const std::string& text, GUID& guid)
	{
		std::wstring wide(text.begin(), text.end());
		return SUCCEEDED(CLSIDFromString(wide.c_str(), &guid));
	}

	const std::vector<DeviceChoice>& UiDevices() { return uiDevices; }

	void RefreshUiDevices()
	{
        ConsumerLifecycle::Gate::Lease lease(ConsumerLifecycle::Runtime());
        if (!lease) return;
		uiDevices.clear();
		if (!LoadApi()) { deviceError = "WheelFfb.dll is missing or incompatible"; return; }
		const int count = ffb.EnumerateDevices();
		for (int i = 0; i < count; ++i)
		{
			GUID guid{};
			char name[260]{};
			wchar_t text[40]{};
			if (!ffb.GetDeviceGuid(i, &guid) || !ffb.GetDeviceName(i, name, sizeof(name)) ||
				!StringFromGUID2(guid, text, 40)) continue;
			std::string identity;
			for (const auto* p = text; *p; ++p) identity += static_cast<char>(*p);
			uiDevices.push_back({ identity, name });
		}
		if (!initialized) { initAttempted = false; deviceError.clear(); }
	}

	static void ApplySelectionChanged(bool resumePaused = false)
	{
        ConsumerLifecycle::Gate::Lease lease(ConsumerLifecycle::Runtime(), resumePaused);
        if (!lease) return;
        // LifecycleIdle already silenced this paused handle. Normal producer
        // output remains inhibited until its release/reset has completed.
        if (!resumePaused) ZeroAllForces();
		if (ffbLoaded && initialized) ffb.FreeDirectInput();
		initialized = false;
		initAttempted = false;
		periodicsActive = false;
		slotRoadTexture = slotTireSlip = -1;
		prevConstantLevel = prevStructLevel = 0;
		warmupFrames = 0;
		deviceError.clear();
		delete sharedModel; sharedModel = nullptr;
		delete sharedShaper; sharedShaper = nullptr;
		useSharedModel = false;
	}

    static bool selectionPending = false;
    void LifecycleIdle()
    {
        ConsumerLifecycle::Gate::Lease lease(ConsumerLifecycle::Runtime(), true);
        if (!lease) return;
        if (ConsumerLifecycle::Runtime().Current() == ConsumerLifecycle::Gate::Phase::Paused) {
            if (initialized && ffbLoaded && !panicStopped) ffb.ZeroForces();
            prevConstantLevel = prevStructLevel = 0;
            warmupFrames = 0;
        }
        const bool resumePaused = ConsumerLifecycle::Runtime().ResumeRequested();
        if (selectionPending && (resumePaused ||
            ConsumerLifecycle::Runtime().Current() == ConsumerLifecycle::Gate::Phase::Running)) {
            selectionPending = false;
            ApplySelectionChanged(resumePaused);
        }
    }
    void SelectionChanged()
    {
        const bool reentrant = ConsumerLifecycle::Gate::Reentrant();
        ConsumerLifecycle::Gate::Lease lease(ConsumerLifecycle::Runtime());
        if (!lease) return;
        if (reentrant) {
            selectionPending = true;
            ConsumerLifecycle::Runtime().DeferUntilIdle();
            return;
        }
        ApplySelectionChanged();
    }

	std::string UiDeviceLabel()
	{
        ConsumerLifecycle::Gate::Lease lease(ConsumerLifecycle::Runtime());
        if (!lease) return "Stopped for game exit";
		if (Settings::FFBDeviceGuid == "steering")
		{
			const auto input = DInputRemap::ReadUiSnapshot();
			return "Use steering wheel - " + (input.connected ? input.name : std::string("unavailable"));
		}
		if (Settings::FFBDeviceGuid == "legacy-index") return "Choose a device to replace the old index selection";
		for (const auto& device : uiDevices)
			if (device.guid == Settings::FFBDeviceGuid) return device.name;
		return (Settings::FFBDeviceName.empty() ? std::string("Saved device") : Settings::FFBDeviceName) + " (disconnected)";
	}

	bool DeferredInit()
	{
        ConsumerLifecycle::Gate::Lease lease(ConsumerLifecycle::Runtime());
        if (!lease) return false;
		if (initAttempted) return initialized;
		initAttempted = true;
		if (!LoadApi()) { deviceError = "WheelFfb.dll is missing or incompatible"; return false; }
		const HWND hwnd = Game::GameHwnd();
		DWORD owner = 0;
		if (!hwnd || !IsWindow(hwnd) || !GetWindowThreadProcessId(hwnd, &owner) || owner != GetCurrentProcessId())
		{
			deviceError = "Game window unavailable; restart the game";
			return false;
		}

		// The same physical device the remap layer polls, but our OWN exclusive
		// handle - poll-side Acquire() churn must never invalidate our downloaded
		// effects. Selected by instance GUID rather than by name, because a
		// Fanatec base presents two identically named devices and only one of
		// them has the actuator.
		GUID guid = {};
		if (Settings::FFBDeviceGuid == "steering")
		{
			GUID saved{};
			if (!Settings::UseDirectInputRemap || !DInputRemap::IsPrimaryInitialized() ||
				!ParseDeviceGuid(Settings::DIRemapDeviceGuid, saved) || !DInputRemap::GetPrimaryDeviceGuid(&guid) ||
				!IsEqualGUID(saved, guid))
			{
				deviceError = "Bind Steering in Controls to save the intended wheel first";
				return false;
			}
		}
		else if (!ParseDeviceGuid(Settings::FFBDeviceGuid, guid))
		{
			deviceError = "Choose an FFB device; old index selection cannot identify a wheel safely";
			return false;
		}
		ffb.SetPreferredDevice(nullptr);
		ffb.SetPreferredDeviceIndex(-1);
		ffb.SetPreferredDeviceGuid(&guid);
		ffb.SetStrictDeviceSelection(1);

		int count = ffb.EnumerateDevices();
		for (int i = 0; i < count; i++)
		{
			char name[260] = {};
			ffb.GetDeviceName(i, name, sizeof(name));
			spdlog::info("FFB: force-feedback device [{}]: '{}'", i, name);
		}

        if (ConsumerLifecycle::Runtime().Current() != ConsumerLifecycle::Gate::Phase::Running ||
            !ConsumerLifecycle::ReadyForActuator(hwnd)) return false;
		if (!ffb.InitDirectInput((int)(INT_PTR)hwnd))
		{
			deviceError = "Selected wheel is unavailable or refused output. Check it, then Refresh devices";
			spdlog::error("FFB: no usable force-feedback device (last HRESULT 0x{:08X})",
				(unsigned)ffb.GetLastHResult());
			return false;
		}
        // Init may pump a reentrant close/session request. Mark the handle as
        // owned for deferred silence, but never start output after that request.
        initialized = true;
        if (ConsumerLifecycle::Runtime().Current() != ConsumerLifecycle::Gate::Phase::Running ||
            !ConsumerLifecycle::ReadyForActuator(hwnd)) return false;
		// Start through an accepted neutral setter, never by replaying whatever
		// parameters a retained native effect may hold. A refused zero is an
		// initialization failure; do not create auxiliary effects or retry every
		// game tick. Refresh/selection is the explicit retry path.
		if (!ffb.SetDeviceForcesXY(0, 0))
		{
			ffb.FreeDirectInput();
			initialized = false;
			periodicsActive = false;
			slotRoadTexture = slotTireSlip = -1;
			prevConstantLevel = prevStructLevel = 0;
			deviceError = "Wheel refused neutral startup; output released. Check the wheel, then Refresh devices";
			return false;
		}

		// Hardware periodics for road texture and tyre slip. -1 from either means
		// the driver exposes no periodic effects, and the constant-force synthesis
		// fallback below carries the vibration instead.
		if (Settings::FFBUsePeriodicEffects)
		{
            if (ConsumerLifecycle::Runtime().Current() != ConsumerLifecycle::Gate::Phase::Running ||
                !ConsumerLifecycle::ReadyForActuator(hwnd)) return false;
			slotRoadTexture = ffb.CreatePeriodicEffect(25);
            if (ConsumerLifecycle::Runtime().Current() != ConsumerLifecycle::Gate::Phase::Running ||
                !ConsumerLifecycle::ReadyForActuator(hwnd)) return false;
			slotTireSlip = ffb.CreatePeriodicEffect(40);
			periodicsActive = (slotRoadTexture >= 0 && slotTireSlip >= 0);
			if (periodicsActive)
				spdlog::info("FFB: periodic effects on slots {} and {}", slotRoadTexture, slotTireSlip);
			else
				spdlog::warn("FFB: Periodic effects unavailable -- using constant-force vibration fallback (15 Hz cap, post-compressor injection)");
		}

		// --- force model: this file's own, or a named profile from the toolkit ---
		// A profile that will not load must never cost force feedback; fall back
		// to the built-in model and say so.
		if (!Settings::FFBProfile.empty() && _stricmp(Settings::FFBProfile.c_str(), "legacy") != 0)
		{
			char dir[MAX_PATH] = {};
			GetModuleFileNameA(Module::DllHandle, dir, MAX_PATH);
			if (char* slash = strrchr(dir, '\\')) *slash = 0;

			std::string why;
			if (dbce::force::load_profile_dir(dir, Settings::FFBProfile, sharedProfile, &why))
			{
				sharedModel = new dbce::force::Model(sharedProfile.model);
				sharedShaper = new dbce::force::Shaper(sharedProfile.shaper);
				useSharedModel = true;
				spdlog::info("FFB: force profile '{}' - {}", sharedProfile.id(), sharedProfile.description);
				for (size_t i = 0; i < sharedProfile.unknown_keys.size(); i++)
					spdlog::warn("FFB: profile key not understood by this build: {}", sharedProfile.unknown_keys[i]);
			}
			else
			{
				spdlog::error("FFB: force profile '{}' not loaded ({}) -- using the built-in model",
					Settings::FFBProfile, why);
			}
		}
		else
		{
			spdlog::info("FFB: using the built-in (legacy) force model");
		}

		initialized = true;
		deviceError.clear();
		spdlog::info("FFB: Initialization complete (WheelFfb)");
		return true;
	}


	int updateCounter = 0;

	// Phase accumulators for the constant-force FALLBACK vibration synthesis
	// (only used when hardware periodic effects are unavailable)
	float roadPhase = 0.0f;
	float slipPhase = 0.0f;

	// Check if the game is in a state where FFB should be active
	static bool IsInGameplay()
	{
		if (!Game::current_mode) return false;
		GameState state = (GameState)*Game::current_mode;
		return state == STATE_GAME ||
			state == STATE_START ||
			state == STATE_GOAL ||
			state == STATE_TIMEUP ||
			state == STATE_SMPAUSEMENU;
	}

	// Called from a broad game hook to zero forces if Update() hasn't
	// been called recently (handles menu transitions where GamePlCar_Ctrl stops).
	void CheckWatchdog()
	{
        ConsumerLifecycle::Gate::Lease lease(ConsumerLifecycle::Runtime());
        if (!lease) return;
		if (!initialized || !ffbLoaded || panicStopped)
			return;

		DWORD now = GetTickCount();
		DWORD elapsed = now - lastUpdateTick;

		// If Update() hasn't been called for 250ms and forces are non-zero, zero them
		if (elapsed > 250 && lastUpdateTick > 0 && prevConstantLevel != 0)
		{
			ZeroAllForces();
			smoothedLateral = 0.0f;
			crashImpulseTimer = 0;
			spdlog::info("FFB: Watchdog zeroed forces (no Update for {}ms)", elapsed);
		}
	}

static void SampleSurface(EVWORK_CAR* car, float& roughness, DWORD& waterFlag)
{
		// ---- Surface roughness from the game's own per-surface table ----
		// sub_1149C0 is the exact LUT the game's Xbox vibration code shipped
		// with: asphalt=0.0 (silent), sand=0.25, grass=0.70, rough=0.85-0.9,
		// water 0.73-0.79 on lake stages (sets waterFlag). Max over 4 tires,
		// same as the original code.
		waterFlag = 0;
		roughness = 0.0f;
		for (int i = 0; i < 4; i++)
		{
			roughness = std::max(roughness, (float)sub_1149C0(
				car->water_flag_24C[i], (int)car->OnRoadPlace_5C.loadColiType_0, &waterFlag));
		}


}

#include "ffb_calculation.inl"

	void Update(EVWORK_CAR* car)
	{
        ConsumerLifecycle::Gate::Lease lease(ConsumerLifecycle::Runtime());
        if (!lease) return;
		if (!car || panicStopped)
			return;

		// Record timestamp for watchdog staleness detection
		lastUpdateTick = GetTickCount();

		// Telemetry shared memory: init once, write every frame (independent of FFB)
		if (!Telemetry::initialized && Settings::TelemetryEnabled)
			Telemetry::Init();

		bool inGameplay = IsInGameplay();
		Telemetry::Write(car, inGameplay);

		// FFB processing only when DirectInputFFB is enabled
		if (!Settings::DirectInputFFB || Overlay::IsActive || Overlay::WheelSettingsVisible ||
			Overlay::IsBindingDialogActive || GetForegroundWindow() != Game::GameHwnd() ||
			!Game::current_mode || *Game::current_mode != STATE_GAME)
		{
			ZeroAllForces();
			warmupFrames = 0;
			return;
		}

		// Lazy initialization: deferred to first game tick
		if (!initialized)
		{
			if (!DeferredInit())
				return;
		}

		// Zero forces when not in gameplay (menus, results, etc.)
		// Prevents the wheel from staying stuck at the last force level
		if (!inGameplay)
		{
			ZeroAllForces();
			smoothedLateral = 0.0f;
			crashImpulseTimer = 0;
			warmupFrames = 0;
			return;
		}

		CalculateSignals(car, 0.0f, 0, {SetConstantForce, UpdatePeriodic}, GetTickCount, SampleSurface);
	}

    // Only the committed outer-loop boundary may call this after leases drain.
    // Native worker completion remains a matched-toolkit release gate.
    void FinalizeForExit()
    {
        panicStopped = true;
        if (ffbLoaded) { ffb.PanicStop(); ffb.FreeDirectInput(); }
        initialized = false;
        periodicsActive = false;
        slotRoadTexture = slotTireSlip = -1;
        prevConstantLevel = prevStructLevel = 0;
        delete sharedModel; sharedModel = nullptr;
        delete sharedShaper; sharedShaper = nullptr;
        useSharedModel = false;
        Telemetry::Shutdown();
        // Successful native module stays pinned; never hot-unload it.
    }

    void SilenceForLifecycle()
    {
        ConsumerLifecycle::Gate::Lease lease(ConsumerLifecycle::Runtime(), true);
        if (!lease || !initialized || !ffbLoaded || panicStopped) return;
        ffb.ZeroForces();
        prevConstantLevel = prevStructLevel = 0;
        warmupFrames = 0;
    }

	const char* UiStatus()
	{
        ConsumerLifecycle::Gate::Lease lease(ConsumerLifecycle::Runtime());
        if (!lease) return "Stopped for game exit";
		if (!Settings::DirectInputFFB) return "Off - choose On to resume";
		if (panicStopped) return "Stopped for game exit";
		if (!deviceError.empty()) return deviceError.c_str();
		if (initAttempted && !initialized) return "Unavailable - check the wheel and WheelFfb.dll, then restart";
		if (Overlay::IsActive || Overlay::WheelSettingsVisible || Overlay::IsBindingDialogActive)
			return "Paused while settings are open";
		if (GetForegroundWindow() != Game::GameHwnd()) return "Paused while the game is unfocused";
		if (!Game::current_mode || *Game::current_mode != STATE_GAME) return "Waiting for driving control";
		return initialized ? "Output enabled (wheel feel not verified)" : "Waiting for the first driving update";
	}
}

// ====================================================================
// Hook class -- self-registering via static instance
// ====================================================================
namespace TickDiscovery { void Observe(EVWORK_CAR* car, bool post); void NoteHooks(bool carHook, bool vibrationCarHookEnabled); }

class DirectInputFFBHook : public Hook
{
	const static int GamePlCar_Ctrl_Addr = 0xA8330;

	inline static SafetyHookInline GamePlCar_Ctrl = {};
	static void __cdecl GamePlCar_Ctrl_Hook(EVWORK_CAR* car)
	{
		// Read-only discovery sides of the car tick; inert unless an external request armed a window.
		TickDiscovery::Observe(car, false);
		FFB::Update(car);
		GamePlCar_Ctrl.call(car);
		TickDiscovery::Observe(car, true);
	}

public:
	std::string_view description() override
	{
		return "DirectInputFFB";
	}

	bool validate() override
	{
		// F6 can enable either feature after startup. This hook alone does not
		// initialize/acquire the wheel or emit telemetry.
		return Settings::OverlayEnabled || Settings::DirectInputFFB || Settings::TelemetryEnabled;
	}

	bool apply() override
	{
		// Only install the inline hook here -- FFB device init is deferred
		// to the first game tick (DirectInput needs a valid HWND).
		// FFB device initialization is deferred to the first Update() call.
		auto targetAddr = Module::exe_ptr(GamePlCar_Ctrl_Addr);
		GamePlCar_Ctrl = safetyhook::create_inline(targetAddr, GamePlCar_Ctrl_Hook);
		if (!GamePlCar_Ctrl)
		{
			spdlog::error("DirectInputFFB: Failed to hook GamePlCar_Ctrl");
			return false;
		}

		TickDiscovery::NoteHooks(true, Settings::VibrationMode != 0 || Settings::UseNewInput);
		spdlog::info("DirectInputFFB: Hook installed (FFB init deferred to first game tick)");
		return true;
	}

	static DirectInputFFBHook instance;
};
DirectInputFFBHook DirectInputFFBHook::instance;
