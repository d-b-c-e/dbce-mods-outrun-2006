#pragma once
// Rig-profile controls (dbce-wheel-mod-toolkit STD-033, docs/controls-contract.md) for the DirectInput remap.
//
// Wheelkit stages the profile in OutRun2006Tweaks.user.ini as a [WheelkitProfile] section, the toolkit's [Controls]
// body under another name: Tweaks already owns [Controls]. At startup, before the INIs are read, a new revision becomes
// the remap's own keys in the same file ([DirectInput], [DirectInput.Calibration], [DirectInput.Shifter],
// [DirectInput.Aux]), the keys the F6 panel saves, in one atomic edit. [ControlsApplied] records the revision, so later
// F6 changes stand until the profile changes. Standalone (no game, plugin or log dependency), so
// tests/profile_controls_test.cpp runs it offline.
#include <filesystem>
#include <string>
#include <vector>

namespace ProfileControls
{
constexpr const char* ProfileSection = "WheelkitProfile";

struct Key { std::string section, key, value; };

struct Plan
{
    bool ok = false;           // a schema-1 section with at least one key to write
    std::string error;         // why nothing applies
    std::string profile, revision;
    std::vector<Key> keys;
    std::vector<std::string> notes; // "action: reason" for each profile entry not applied
};

// Pure: [Controls] body lines -> keys. useNewInput is Tweaks' [Controls] UseNewInput as it stands (the remap is
// inactive while it is on, so nothing is applied then).
Plan PlanLines(const std::vector<std::string>& lines, bool useNewInput);

// Reads the [WheelkitProfile] body from the user INI; false when the file or section is absent.
bool ReadProfile(const std::filesystem::path& userIni, std::vector<std::string>& body);

// UseNewInput as the game will read it: the user INI wins over the main INI; default false.
bool UseNewInput(const std::filesystem::path& mainIni, const std::filesystem::path& userIni);

// True when the user INI's [ControlsApplied] does not record plan.revision.
bool Pending(const std::filesystem::path& userIni, const Plan& plan);

// One transaction: the user INI is backed up once (<user>.before-profile-controls), every key and [ControlsApplied]
// go into one document that is read back, then it replaces the user INI in one move. On failure the file keeps its
// bytes. fault (tests only): 1 fails the temporary write, 2 the replace.
bool Write(const std::filesystem::path& userIni, const Plan& plan, int fault = 0);

// Startup: plan, check, write when pending. Returns the log lines to print; applied is set when keys were written.
std::vector<std::string> ApplyAtStartup(const std::filesystem::path& gameDir, bool& applied);
}
