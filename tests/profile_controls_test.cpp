// Offline checks for src/profile_controls.cpp (STD-033 rig-profile controls -> the DirectInput remap's user INI keys).
// Built and run by tools/Test-ProfileControls.ps1 (MSVC x86, the game's architecture). Reads go through the game's own
// inih INIReader, as Settings::read does. No game, device or force.
#include "profile_controls.hpp"
#include "pov_binding.hpp"

#include <ini.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace ProfileControls;

static int g_checks, g_failed;
static void check(bool ok, const std::string& what)
{
    ++g_checks;
    if (!ok) { ++g_failed; std::printf("FAIL %s\n", what.c_str()); }
}

#define WHEEL "dev={11111111-2222-3333-4444-555555555555} prod={0006346e-0000-0000-0000-504944564944}"
#define SHIFTER "dev={66666666-7777-8888-9999-aaaaaaaaaaaa} prod={05310483-0000-0000-0000-504944564944}"
#define OTHER "dev={99999999-9999-9999-9999-999999999999} prod={05310483-0000-0000-0000-504944564944}"
#define WHEEL_UPPER "{11111111-2222-3333-4444-555555555555}"
#define SHIFTER_UPPER "{66666666-7777-8888-9999-AAAAAAAAAAAA}"
#define OTHER_UPPER "{99999999-9999-9999-9999-999999999999}"

static std::vector<std::string> profile(const std::string& transmission = "Sequential")
{
    return {
        "Schema = 1", "Profile = Test Rig", "Revision = rev-1", "Transmission = " + transmission,
        "steer = axis 0 " WHEEL " range=0..65535 rest=33265 travel=-1 name=\"MOZA R12 Base\"",
        "throttle = axis 2 " WHEEL " range=0..65535 rest=0 travel=+1 name=\"MOZA R12 Base\"",
        "brake = axis 5 " WHEEL " range=0..65535 rest=1743 travel=+1 name=\"MOZA R12 Base\"",
        "clutch = axis 6 " WHEEL " range=0..65535 rest=0 travel=+1 name=\"MOZA R12 Base\"",
        "handbrake = axis 7 " WHEEL " range=0..65535 rest=0 travel=+1 name=\"MOZA R12 Base\"",
        "shiftUp = button 9 " SHIFTER " name=\"DS-8X Shifter\"",
        "shiftDown = button 10 " SHIFTER " name=\"DS-8X Shifter\"",
        "reverse = button 7 " SHIFTER " name=\"DS-8X Shifter\"",
        "confirm = button 31 " WHEEL, "back = button 18 " WHEEL, "start = button 35 " WHEEL, "select = button 22 " WHEEL,
        "navUp = hat 0 0 " WHEEL, "navDown = hat 0 18000 " WHEEL, "navLeft = hat 0 27000 " WHEEL, "navRight = hat 0 9000 " WHEEL,
        "camera = button 32 " WHEEL, "lookBack = button 10 " WHEEL, "reset = button 33 " WHEEL, "horn = button 1 " WHEEL,
        "gear1 = button 0 " SHIFTER, "gear2 = button 1 " SHIFTER, "gear3 = button 2 " SHIFTER, "gear4 = button 3 " SHIFTER,
        "gear5 = button 4 " SHIFTER, "gear6 = button 5 " SHIFTER, "gear7 = button 6 " SHIFTER,
    };
}

static std::string key(const Plan& p, const std::string& section, const std::string& k)
{
    for (const Key& e : p.keys) if (e.section == section && e.key == k) return e.value;
    return "<absent>";
}
static bool noted(const Plan& p, const std::string& prefix)
{
    for (const std::string& n : p.notes) if (n.compare(0, prefix.size(), prefix) == 0) return true;
    return false;
}
static Plan one(const std::string& line, const Current& current = {})
{
    std::vector<std::string> lines = {"Schema = 1", "Transmission = Sequential",
        "steer = axis 0 " WHEEL " range=0..65535 rest=32768 travel=-1"};
    if (!line.empty()) lines.push_back(line);
    return PlanLines(lines, current);
}
static std::string slurp(const fs::path& p)
{
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}
static void spit(const fs::path& p, const std::string& text) { std::ofstream(p, std::ios::binary) << text; }
static std::string read(const fs::path& p, const char* section, const char* k)
{
    try { inih::INIReader ini(p); return ini.Get<std::string>(section, k); } catch (...) { return "<unread>"; }
}

int main()
{
    // The owner-shaped profile.
    Plan p = PlanLines(profile(), {});
    check(p.ok && p.revision == "rev-1" && p.profile == "Test Rig", "profile plans");
    check(key(p, "DirectInput", "UseDirectInputRemap") == "true", "remap on");
    check(key(p, "DirectInput", "DeviceGuid") == WHEEL_UPPER, "primary device = steering wheel");
    check(key(p, "DirectInput", "SteeringAxis") == "0" && key(p, "DirectInput", "SteeringInvert") == "false", "steering axis");
    check(key(p, "DirectInput.Calibration", "SteeringEnabled") == "false", "steering uses the range centre");
    check(key(p, "DirectInput", "AccelerationAxis") == "2" && key(p, "DirectInput", "AccelerationInvert") == "false", "throttle axis");
    check(key(p, "DirectInput", "ThrottleDeviceGuid") == WHEEL_UPPER && key(p, "DirectInput", "ThrottleDeviceName") == "MOZA R12 Base", "throttle device");
    check(key(p, "DirectInput.Calibration", "ThrottleEnabled") == "true" && key(p, "DirectInput.Calibration", "ThrottleMinimum") == "0" &&
          key(p, "DirectInput.Calibration", "ThrottleMaximum") == "65535", "throttle calibration");
    check(key(p, "DirectInput", "BrakeAxis") == "5" && key(p, "DirectInput", "BrakeInvert") == "false", "brake axis");
    check(key(p, "DirectInput.Calibration", "BrakeMinimum") == "1743" && key(p, "DirectInput.Calibration", "BrakeMaximum") == "65535" &&
          key(p, "DirectInput.Calibration", "BrakeCenter") == "33639", "brake calibration starts at the captured rest");
    check(key(p, "DirectInput", "ButtonA") == "31" && key(p, "DirectInput", "ButtonB") == "18", "confirm/back");
    check(key(p, "DirectInput", "ButtonStart") == "35" && key(p, "DirectInput", "ButtonBack") == "22", "start/select");
    check(key(p, "DirectInput", "ButtonChangeView") == "32", "camera");
    check(key(p, "DirectInput", "ButtonSelUp") == "128" && key(p, "DirectInput", "ButtonSelRight") == "129" &&
          key(p, "DirectInput", "ButtonSelDown") == "130" && key(p, "DirectInput", "ButtonSelLeft") == "131", "hat menus as POV directions");
    check(key(p, "DirectInput", "ButtonGearUp") == "<absent>" && key(p, "DirectInput", "ButtonX") == "<absent>",
          "primary keys outside the profile untouched while the primary device is unset");
    check(key(p, "DirectInput.Shifter", "DeviceGuid") == SHIFTER_UPPER, "shifter slot device");
    check(key(p, "DirectInput.Shifter", "ButtonGearUp") == "9" && key(p, "DirectInput.Shifter", "ButtonGearDown") == "10", "sequential shifts");
    check(key(p, "DirectInput.Shifter", "ButtonGear1") == "0" && key(p, "DirectInput.Shifter", "ButtonGear6") == "5" &&
          key(p, "DirectInput.Shifter", "ButtonGearReverse") == "7", "gears and reverse");
    check(key(p, "DirectInput.Shifter", "GearMode") == "sequential", "gear mode follows the transmission");
    check(noted(p, "gear7: no OutRun 2006 control") && noted(p, "clutch: no OutRun 2006 control") &&
          noted(p, "handbrake: no OutRun 2006 control") && noted(p, "lookBack: no OutRun 2006 control") &&
          noted(p, "reset: no OutRun 2006 control") && noted(p, "horn: no OutRun 2006 control"), "unsupported actions noted");

    // Transmission transitions are explicit: GearMode is always written (Astra 3528 finding 1).
    check(key(PlanLines(profile("HPattern"), {}), "DirectInput.Shifter", "GearMode") == "hpattern", "complete H-pattern");
    Plan automatic = PlanLines(profile("Automatic"), {});
    check(key(automatic, "DirectInput.Shifter", "GearMode") == "sequential" && noted(automatic, "transmission:"), "automatic writes sequential, not a stale hpattern");
    Plan noShifter = one("");
    check(key(noShifter, "DirectInput.Shifter", "GearMode") == "sequential", "a profile without a shifter still clears an old H-pattern");
    Plan incomplete = PlanLines({"Schema = 1", "Transmission = HPattern", "steer = axis 0 " WHEEL " range=0..65535 rest=32768 travel=-1",
                                 "gear2 = button 1 " SHIFTER}, {});
    check(key(incomplete, "DirectInput.Shifter", "GearMode") == "sequential" && noted(incomplete, "transmission: H-pattern needs"), "H-pattern without gear 1");
    Plan clash = PlanLines({"Schema = 1", "Transmission = HPattern", "steer = axis 0 " WHEEL " range=0..65535 rest=32768 travel=-1",
                            "gear1 = button 1 " SHIFTER, "gear2 = button 1 " SHIFTER}, {});
    check(key(clash, "DirectInput.Shifter", "GearMode") == "sequential", "H-pattern with a shared gear button");

    // Slot reassignment keeps no stale button number (finding 2).
    Current aux; aux.auxDev = OTHER_UPPER;
    Plan moved = one("confirm = button 3 " SHIFTER, aux);
    check(key(moved, "DirectInput.Aux", "DeviceGuid") == SHIFTER_UPPER && key(moved, "DirectInput.Aux", "ButtonA") == "3", "aux on the new device");
    check(key(moved, "DirectInput.Aux", "ButtonStart") == "-1" && noted(moved, "DirectInput.Aux ButtonStart: cleared"), "the old aux device's Start cleared");
    Current sameAux; sameAux.auxDev = "{66666666-7777-8888-9999-aaaaaaaaaaaa}";
    check(key(one("confirm = button 3 " SHIFTER, sameAux), "DirectInput.Aux", "ButtonStart") == "<absent>", "same aux device (any case): nothing cleared");
    Plan firstAux = one("confirm = button 3 " SHIFTER);
    check(key(firstAux, "DirectInput.Aux", "ButtonStart") == "-1", "a slot enabled for the first time keeps no inactive numbers");
    Current oldShifter; oldShifter.shifterDev = OTHER_UPPER;
    Plan shifted = one("shiftUp = button 9 " SHIFTER, oldShifter);
    check(key(shifted, "DirectInput.Shifter", "ButtonGear3") == "-1" && key(shifted, "DirectInput.Shifter", "ButtonGearUp") == "9", "stale shifter gates cleared");
    Current oldPrimary; oldPrimary.primaryDev = OTHER_UPPER;
    Plan wheelSwap = one("confirm = button 31 " WHEEL, oldPrimary);
    check(key(wheelSwap, "DirectInput", "ButtonX") == "-1" && key(wheelSwap, "DirectInput", "ButtonA") == "31", "a different primary wheel keeps no old buttons");
    Current samePrimary; samePrimary.primaryDev = WHEEL_UPPER;
    check(key(one("confirm = button 31 " WHEEL, samePrimary), "DirectInput", "ButtonX") == "<absent>", "same primary wheel: extras kept");

    // POV directions: the encoding and the contract's hat rule (a diagonal presses both neighbours).
    check(PovBinding::Encode(0, 27000) == 131 && PovBinding::Encode(1, 0) == 132 && PovBinding::Encode(3, 9000) == 141, "POV encode");
    check(PovBinding::Encode(4, 0) == -1 && PovBinding::Encode(0, 4500) == -1 && PovBinding::Encode(0, 36000) == -1, "POV encode refusals");
    check(PovBinding::Pressed(0, 128) && !PovBinding::Pressed(0, 129) && PovBinding::Pressed(9000, 129), "POV straight");
    check(PovBinding::Pressed(4500, 128) && PovBinding::Pressed(4500, 129) && !PovBinding::Pressed(4500, 130), "POV diagonal presses both");
    check(PovBinding::Pressed(31500, 128) && PovBinding::Pressed(31500, 131), "POV wraps at 36000");
    check(!PovBinding::Pressed(0xFFFFFFFFul, 128) && !PovBinding::Pressed(0x0000FFFFul, 130) && !PovBinding::Pressed(0, 127) &&
          !PovBinding::Pressed(0, 144), "centred POV and non-POV bindings");
    Plan t = one("navUp = hat 0 4500 " WHEEL);
    check(key(t, "DirectInput", "ButtonSelUp") == "<absent>" && noted(t, "navUp: a diagonal hat direction"), "diagonal hat refused");
    t = one("confirm = hat 2 18000 " SHIFTER);
    check(key(t, "DirectInput.Aux", "ButtonA") == "138", "a hat on another device -> Aux POV");

    // Pedal direction: calibration ends and invert for either travel and the inverted flag.
    t = one("throttle = axis 2 " WHEEL " range=0..65535 rest=65535 travel=-1");
    check(key(t, "DirectInput", "AccelerationInvert") == "true" && key(t, "DirectInput.Calibration", "ThrottleMinimum") == "0" &&
          key(t, "DirectInput.Calibration", "ThrottleMaximum") == "65535", "travel -1");
    t = one("throttle = axis 2 " WHEEL " range=0..65535 rest=4000 travel=+1 inverted");
    check(key(t, "DirectInput", "AccelerationInvert") == "true" && key(t, "DirectInput.Calibration", "ThrottleMinimum") == "4000", "inverted, travel +1");
    t = one("brake = axis 5 " WHEEL " range=0..65535 rest=60000 travel=-1 inverted");
    check(key(t, "DirectInput", "BrakeInvert") == "false" && key(t, "DirectInput.Calibration", "BrakeMaximum") == "60000", "inverted, travel -1");
    t = one("brake = axis 5 " SHIFTER " range=0..65535 rest=0 travel=+1");
    check(key(t, "DirectInput", "BrakeDeviceGuid") == SHIFTER_UPPER, "a pedal on its own device");

    // Refusals.
    t = one("throttle = button 3 " WHEEL);
    check(key(t, "DirectInput", "AccelerationAxis") == "<absent>" && noted(t, "throttle: a button pedal"), "button pedal");
    t = one("throttle = axis 2 " WHEEL " range=-32768..32767 rest=-32768 travel=+1");
    check(key(t, "DirectInput", "AccelerationAxis") == "<absent>" && noted(t, "throttle: range"), "pedal range");
    t = one("brake = axis 5 " WHEEL " range=0..65535 rest=60000 travel=+1");
    check(key(t, "DirectInput", "BrakeAxis") == "<absent>" && noted(t, "brake: travel from rest"), "short pedal travel");
    t = PlanLines({"Schema = 1", "steer = axis 0 " WHEEL " range=0..65535 rest=32768 travel=-1", "gear1 = button 0 " SHIFTER,
                   "gear2 = button 1 " OTHER}, {});
    check(key(t, "DirectInput.Shifter", "ButtonGear2") == "-1" && noted(t, "gear2: the remap's DirectInput.Shifter slot already holds"),
          "one shifter device; the refused gear's old number is cleared too");
    Current newInput; newInput.useNewInput = true;
    t = one("", newInput);
    check(!t.ok && t.error.find("UseNewInput") != std::string::npos, "UseNewInput leaves everything");
    t = PlanLines({"Schema = 1", "steer = axis 0 " WHEEL " range=-32768..32767 rest=0 travel=-1"}, {});
    check(!t.ok && t.error.find("steering range") != std::string::npos, "steering range");
    t = PlanLines({"Schema = 2", "steer = axis 0 " WHEEL " range=0..65535 rest=32768 travel=-1"}, {});
    check(!t.ok && t.error.find("Schema 2") != std::string::npos, "future schema");
    t = PlanLines({"Schema = 1", "confirm = button 31 " WHEEL}, {});
    check(!t.ok && t.error.find("no steer") != std::string::npos, "no steering");
    t = one("select =");
    check(noted(t, "select: unbound in the profile"), "explicitly unbound keeps the remap's value");

    const fs::path dir = fs::temp_directory_path() / "outrun-profile-controls-test";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);
    const fs::path user = dir / "OutRun2006Tweaks.user.ini", mainIni = dir / "OutRun2006Tweaks.ini",
        backup = dir / "OutRun2006Tweaks.user.ini.before-profile-controls", tmp = dir / "OutRun2006Tweaks.user.ini.controls-tmp";

    // The current state as the game reads it (finding 4): inih booleans, inline comments, invalid values, user over main.
    auto current = [&](const std::string& mainText, const std::string& userText) {
        spit(mainIni, mainText);
        if (userText.empty()) fs::remove(user, ec); else spit(user, userText);
        return ReadCurrent(mainIni, user);
    };
    check(current("[Controls]\nUseNewInput = yes\n", "").useNewInput, "yes is true");
    check(current("[Controls]\nUseNewInput = on ; comment\n", "").useNewInput, "inline comment stripped");
    check(current("[Controls]\nUseNewInput = true\n", "[Controls]\nUseNewInput = maybe\n").useNewInput, "an invalid user value keeps the main one");
    check(!current("[Controls]\nUseNewInput = true\n", "[Controls]\nUseNewInput = off\n").useNewInput, "user over main");
    check(current("", "[DirectInput.Aux]\nDeviceGuid = " OTHER_UPPER "\n").auxDev == OTHER_UPPER, "slot owner read");
    check(current("", "[directinput]\nDeviceGuid = " OTHER_UPPER "\n").primaryDev.empty(), "the game does not read [directinput]");
    fs::remove(mainIni, ec);

    // The user INI transaction.
    const std::string owner =
        "[Controls]\r\nUseNewInput = false\r\nSteeringDeadZone = 0\r\n[DirectInput]\r\nUseDirectInputRemap = true\r\nBrakeAxis = 5\r\n"
        "AccelerationAxis = 2\r\nButtonGearUp = 13\r\nButtonGearDown = 12\r\nButtonStart = 35\r\nButtonSelUp = 8\r\n[WheelSettings]\r\n"
        "View = Simple\r\n[DirectInput.Calibration]\r\nBrakeCenter = 32767.5\r\nBrakeEnabled = true\r\nBrakeMaximum = 65535\r\nBrakeMinimum = 0\r\n"
        "[FFB]\r\nDirectInputFFB = true\r\n";
    std::string why;
    fs::remove(backup, ec);
    for (int pass = 0; pass < 2; ++pass) {
        if (pass) spit(backup, owner);
        for (int fault = 1; fault <= 2; ++fault) {
            spit(user, owner);
            check(!Write(user, p, why, fault), "fault " + std::to_string(fault) + " fails");
            check(slurp(user) == owner, "fault " + std::to_string(fault) + " keeps the bytes");
            check(!fs::exists(tmp), "no temporary file left");
            check(fs::exists(backup) == (pass == 1), "backup only when it existed");
        }
    }
    fs::remove(backup, ec);
    spit(user, owner);
    check(Write(user, p, why), "clean write (" + why + ")");
    const std::string applied = slurp(user);
    check(slurp(backup) == owner, "backup holds the original bytes");
    check(applied.find("[Controls]\r\nUseNewInput = false\r\nSteeringDeadZone = 0\r\n[DirectInput]\r\n") == 0, "Tweaks' own [Controls] untouched, CRLF kept");
    check(applied.find("BrakeMinimum = 1743\r\n") != std::string::npos && applied.find("BrakeMinimum = 0") == std::string::npos, "brake minimum replaced in place");
    check(applied.find("ButtonGearUp = 13\r\n") != std::string::npos, "owner keys outside the profile kept");
    check(applied.find("ButtonSelUp = 128\r\n") != std::string::npos && applied.find("ButtonSelUp = 8") == std::string::npos, "the profile's hat replaces the menu button in place");
    check(applied.find("[WheelSettings]\r\nView = Simple\r\n") != std::string::npos && applied.find("[FFB]\r\nDirectInputFFB = true\r\n") != std::string::npos, "other sections kept");
    check(read(user, "DirectInput", "DeviceGuid") == WHEEL_UPPER && read(user, "DirectInput.Shifter", "GearMode") == "sequential" &&
          read(user, "ControlsApplied", "Revision") == "rev-1", "the game's parser reads the applied keys");
    check(!Pending(user, p), "applied revision is not pending");
    check(Write(user, p, why) && slurp(user) == applied, "the same plan again changes nothing");

    // Refused, bytes kept: an owned section under another case; a value the game's parser would read differently.
    const std::string lower = "[directinput]\r\nDeviceGuid = " OTHER_UPPER "\r\n";
    spit(user, lower);
    check(!Write(user, p, why) && why.find("letter case") != std::string::npos && slurp(user) == lower, "[directinput] refused");
    Plan semicolon = PlanLines({"Schema = 1", "Revision = r2", "steer = axis 0 " WHEEL " range=0..65535 rest=32768 travel=-1",
                                "throttle = axis 2 " WHEEL " range=0..65535 rest=0 travel=+1 name=\"MOZA ;R12\""}, {}); // inih cuts an inline comment after whitespace
    spit(user, owner);
    fs::remove(backup, ec);
    check(!Write(user, semicolon, why) && why.find("read-back differs") != std::string::npos && slurp(user) == owner && !fs::exists(tmp),
          "a value inih would cut at ' ;' is refused");

    // Startup apply from the staged section.
    fs::remove(backup, ec);
    std::string staged = owner + "[WheelkitProfile]\r\n";
    for (const std::string& l : profile()) staged += l + "\r\n";
    spit(user, staged);
    bool did = false;
    auto log = ApplyAtStartup(dir, did);
    check(did && !log.empty() && log[0].find("revision rev-1 applied") != std::string::npos, "startup applies");
    check(slurp(user).find("[WheelkitProfile]\r\nSchema = 1\r\n") != std::string::npos, "the staged profile stays in the user INI");
    const std::string once = slurp(user);
    log = ApplyAtStartup(dir, did);
    check(!did && log.empty() && slurp(user) == once, "second start: nothing pending");
    spit(mainIni, "[Controls]\nUseNewInput = true\n");
    const std::string blocked = "[DirectInput]\nUseDirectInputRemap = true\n[WheelkitProfile]\nSchema = 1\nRevision = r9\nsteer = axis 0 " WHEEL " range=0..65535 rest=32768 travel=-1\n";
    spit(user, blocked);
    log = ApplyAtStartup(dir, did);
    check(!did && !log.empty() && log[0].find("UseNewInput") != std::string::npos && slurp(user) == blocked, "main INI UseNewInput=true refuses");
    fs::remove_all(dir, ec);

    std::printf("%s %d profile-controls checks\n", g_failed ? "FAIL" : "PASS", g_checks);
    return g_failed ? 1 : 0;
}
