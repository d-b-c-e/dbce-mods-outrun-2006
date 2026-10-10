#pragma once
// Test injection for the DirectInput remap (dbce-wheel-mod-toolkit STD-033 section 6), development only.
//
// Raw samples replace a slot's DIJOYSTATE2 read in PollSlot (hooks_inputremap.cpp), before every binding, calibration,
// edge and UI read, so the game takes them through the applied profile exactly as it takes the wheel. It arms once, at
// startup right after the profile request is applied (dllmain), only when all of these hold, and stays off otherwise:
//   - the process is signal-muted (DBCE_OUTRUN_SIGNAL_MUTE set at its start, signal_mute_policy.hpp): no force of any
//     kind (the mod's FFB, the game's own effects, rumble) until it exits, whatever the INI or F6 say later;
//   - %LOCALAPPDATA%\dbce\outrun2006\inject.on names a session: "nonce=<8-64 letters/digits>" and
//     "expires=<unix seconds, UTC>" no more than an hour ahead;
//   - the [Controls] profile request in OutRun2006Tweaks.user.ini is applied (ProfileControls: not pending).
// Commands come from %LOCALAPPDATA%\dbce\outrun2006\inject.txt, read once each time it changes (at most every 100 ms,
// on the game's input thread), at most 4096 bytes. Its first line is "nonce=<the session's>"; then at most 32 commands
// in the toolkit grammar:
//   inject raw <axis|button|hat> <index> [angle] dev=<instance> value=<raw> ms=<50-15000> [range=<min>..<max>]
//   inject action <id> <n> ms=<50-15000>      (resolved through the applied [Controls])
// dev= is a DirectInput instance; a sample reaches only the slot that opened that instance. A session takes at most
// 2000 commands and ends at its expiry. Running samples are dropped then, and whenever their device's read fails or
// its slot is released, so they never carry over to a reopened device.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <dinput.h>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace RemapInject
{
	// Startup, after ProfileControls::ApplyAtStartup. Returns the log lines to print (nothing when not requested).
	std::vector<std::string> Init(const std::filesystem::path& gameDir);
	bool Armed();
	// Reads new commands when the file changed (at most every 100 ms).
	void Poll();
	// One successful GetDeviceState of `device` (instance `instance`) into `state`: running samples replace their
	// objects. Returns how many objects carried one.
	int Deliver(const GUID& instance, IDirectInputDevice8A* device, DIJOYSTATE2& state);
	// The instance's read failed or its slot was released: its running samples end.
	void DeviceGone(const GUID& instance);

	// The observer for an injected run, because a signal-muted process publishes no telemetry (SimHub can turn that
	// shared memory into shaker output). While armed, each FFB update copies the game's own car words into
	// "Local\DbceOutRunInjectObserve-<nonce>", a mapping only the run's observer knows: ObserveData below. No UDP, no
	// other reader, nothing when not armed.
#pragma pack(push, 1)
	struct ObserveData
	{
		uint32_t version;      // 1
		uint32_t packetId;     // a sequence: odd while an update's fields are written, even when they are whole.
		                       // A reader keeps a copy only when packetId was the same even value before and after it.
		uint32_t tickMs;       // GetTickCount of the update
		float speed;           // EVWORK_CAR field_1C4, as OutRun2006Telemetry's speed
		float steer;           // EVWORK_CAR field_1D0, as OutRun2006Telemetry's steeringAngle
		int32_t pedal;         // EVWORK_CAR pedal_amount_34: the game's throttle, 0..255
		uint32_t gear;         // EVWORK_CAR cur_gear_208
		uint32_t gameMode;     // Game::current_mode
		uint8_t inGameplay;
		uint8_t pad[3];
	};
#pragma pack(pop)
	static_assert(sizeof(ObserveData) == 36, "ObserveData layout");
	// Returns whether the update was published. The mapping is created by this process or not at all: one that already
	// exists (another writer, or a reader that created it) is refused and nothing is published for the run.
	bool Observe(float speed, float steer, int pedal, uint32_t gear, uint32_t gameMode, bool inGameplay);
	std::string ObserveName();   // "" when not armed

	// Tests only: arm without files or environment (lines = the applied [Controls] body; nonce "test", no expiry), feed
	// one command or a whole command file, end the session at a time, read an inject.on text, use an explicit clock,
	// and deliver into a state with a range function instead of a device.
	bool TestArm(const std::vector<std::string>& controlsLines);
	bool TestCommand(const std::string& line, std::string& why);
	int TestFile(const std::string& text, std::string& why);
	void TestExpire(uint64_t expiresMs);
	bool TestSession(const std::string& text, uint64_t nowUnix, std::string& nonceOrWhy);
	void TestClock(uint64_t nowMs);
	int TestDeliver(const GUID& instance, DIJOYSTATE2& state, int buttons, int povs, long axisMin, long axisMax);
	std::string InstanceText(const GUID& instance);
}
