#pragma once
// Rig-profile controls (dbce-wheel-mod-toolkit STD-033, docs/controls-contract.md) for the DirectInput remap.
//
// Wheelkit stages the profile in OutRun2006Tweaks.user.ini as a [WheelkitProfile] section, the toolkit's [Controls]
// body under another name: Tweaks already owns [Controls]. At startup, before the INIs are read, a new revision becomes
// the remap's own keys in the same file ([DirectInput], [DirectInput.Calibration], [DirectInput.Shifter],
// [DirectInput.Aux]), the keys the F6 panel saves, in one atomic edit. [ControlsApplied] records the revision, so later
// F6 changes stand until the profile changes. Everything the game will read is read with its own parser (inih
// INIReader, as Settings::read: main INI, then the user INI over it), and the new file is read back the same way
// before it replaces the old one. No game, plugin or log dependency, so tests/profile_controls_test.cpp runs it.
#include <filesystem>
#include <string>
#include <vector>

namespace ProfileControls
{
constexpr const char* ProfileSection = "WheelkitProfile";

struct Key { std::string section, key, value; };

// What the game currently reads, as far as the plan depends on it.
struct Current
{
    bool useNewInput = false;                     // Tweaks' [Controls] UseNewInput: the remap is inactive while on
    std::string primaryDev, shifterDev, auxDev;   // each slot's DeviceGuid ("" = unset)
};

struct Plan
{
    bool ok = false;           // a schema-1 section with at least one key to write
    std::string error;         // why nothing applies
    std::string profile, revision;
    std::vector<Key> keys;
    std::vector<std::string> notes; // "action: reason" for each profile entry not applied, and each cleared key
};

// Pure: [WheelkitProfile] body lines + the current state -> keys.
Plan PlanLines(const std::vector<std::string>& lines, const Current& current);

// Reads the [WheelkitProfile] body from the user INI; false when the file or section is absent.
bool ReadProfile(const std::filesystem::path& userIni, std::vector<std::string>& body);

// The current state through the game's parser: main INI, then the user INI over it; an unreadable file is skipped as
// Settings::read skips it, and an invalid value keeps the previous one, as INIReader::Get with a default does.
Current ReadCurrent(const std::filesystem::path& mainIni, const std::filesystem::path& userIni);

// True when the user INI's [ControlsApplied] does not record plan.revision.
bool Pending(const std::filesystem::path& userIni, const Plan& plan);

// One transaction: the user INI is backed up once (<user>.before-profile-controls); every key and [ControlsApplied]
// go into one document; it is written beside the user INI, read back with the game's parser, then moved over it in one
// step. Refused (nothing written) when an owned section exists under another letter case, which the game would not
// read. On any failure the file keeps its bytes; why says what failed. fault (tests only): 1 fails the temporary write,
// 2 the replace.
bool Write(const std::filesystem::path& userIni, const Plan& plan, std::string& why, int fault = 0);

// Startup: plan, check, write when pending. Returns the log lines to print; applied is set when keys were written.
std::vector<std::string> ApplyAtStartup(const std::filesystem::path& gameDir, bool& applied);
}
