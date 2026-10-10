// Offline checks for src/profile_controls.cpp (STD-033 rig-profile controls -> the DirectInput remap's user INI keys).
// Built and run by tools/Test-ProfileControls.ps1 (MSVC x86, the game's architecture). No game, device or force.
#include "profile_controls.hpp"

#include <cstdio>
#include <cstring>
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
#define WHEEL_UPPER "{11111111-2222-3333-4444-555555555555}"
#define SHIFTER_UPPER "{66666666-7777-8888-9999-AAAAAAAAAAAA}"

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
static Plan one(const std::string& line, bool useNewInput = false)
{
    std::vector<std::string> lines = {"Schema = 1", "Transmission = Sequential",
        "steer = axis 0 " WHEEL " range=0..65535 rest=32768 travel=-1"};
    if (!line.empty()) lines.push_back(line);
    return PlanLines(lines, useNewInput);
}
static std::string slurp(const fs::path& p)
{
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}
static void spit(const fs::path& p, const std::string& text) { std::ofstream(p, std::ios::binary) << text; }

int main()
{
    // The owner-shaped profile.
    Plan p = PlanLines(profile(), false);
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
    check(key(p, "DirectInput", "ButtonSelUp") == "<absent>" && noted(p, "navUp: the remap binds buttons only (no POV)"), "hat menus refused, owner's kept");
    check(key(p, "DirectInput", "ButtonGearUp") == "<absent>", "primary paddles untouched (shifts are on the shifter)");
    check(key(p, "DirectInput.Shifter", "DeviceGuid") == SHIFTER_UPPER, "shifter slot device");
    check(key(p, "DirectInput.Shifter", "ButtonGearUp") == "9" && key(p, "DirectInput.Shifter", "ButtonGearDown") == "10", "sequential shifts");
    check(key(p, "DirectInput.Shifter", "ButtonGear1") == "0" && key(p, "DirectInput.Shifter", "ButtonGear6") == "5" &&
          key(p, "DirectInput.Shifter", "ButtonGearReverse") == "7", "gears and reverse");
    check(key(p, "DirectInput.Shifter", "GearMode") == "sequential", "gear mode follows the transmission");
    check(noted(p, "gear7: no OutRun 2006 control") && noted(p, "clutch: no OutRun 2006 control") &&
          noted(p, "handbrake: no OutRun 2006 control") && noted(p, "lookBack: no OutRun 2006 control") &&
          noted(p, "reset: no OutRun 2006 control") && noted(p, "horn: no OutRun 2006 control"), "unsupported actions noted");
    check(key(PlanLines(profile("HPattern"), false), "DirectInput.Shifter", "GearMode") == "hpattern", "H-pattern profile");
    Plan automatic = PlanLines(profile("Automatic"), false);
    check(key(automatic, "DirectInput.Shifter", "GearMode") == "<absent>" && noted(automatic, "transmission:"), "automatic leaves the mode");

    // Pedal direction: calibration ends and invert for either travel and the inverted flag.
    Plan t = one("throttle = axis 2 " WHEEL " range=0..65535 rest=65535 travel=-1");
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
    t = one("confirm = button 3 " SHIFTER);
    check(key(t, "DirectInput.Aux", "ButtonA") == "3" && key(t, "DirectInput.Aux", "DeviceGuid") == SHIFTER_UPPER, "button on another device -> Aux");
    t = PlanLines({"Schema = 1", "steer = axis 0 " WHEEL " range=0..65535 rest=32768 travel=-1", "gear1 = button 0 " SHIFTER,
                   "gear2 = button 1 dev={99999999-9999-9999-9999-999999999999} prod={05310483-0000-0000-0000-504944564944}"}, false);
    check(key(t, "DirectInput.Shifter", "ButtonGear2") == "<absent>" && noted(t, "gear2: the remap's DirectInput.Shifter slot already holds"), "one shifter device");
    t = one("", true);
    check(!t.ok && t.error.find("UseNewInput") != std::string::npos, "UseNewInput leaves everything");
    t = PlanLines({"Schema = 1", "steer = axis 0 " WHEEL " range=-32768..32767 rest=0 travel=-1"}, false);
    check(!t.ok && t.error.find("steering range") != std::string::npos, "steering range");
    t = PlanLines({"Schema = 2", "steer = axis 0 " WHEEL " range=0..65535 rest=32768 travel=-1"}, false);
    check(!t.ok && t.error.find("Schema 2") != std::string::npos, "future schema");
    t = PlanLines({"Schema = 1", "confirm = button 31 " WHEEL}, false);
    check(!t.ok && t.error.find("no steer") != std::string::npos, "no steering");
    t = one("select =");
    check(noted(t, "select: unbound in the profile"), "explicitly unbound keeps the remap's value");

    // The user INI transaction.
    const fs::path dir = fs::temp_directory_path() / "outrun-profile-controls-test";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);
    const fs::path user = dir / "OutRun2006Tweaks.user.ini", backup = dir / "OutRun2006Tweaks.user.ini.before-profile-controls",
        tmp = dir / "OutRun2006Tweaks.user.ini.controls-tmp";
    const std::string owner =
        "[Controls]\r\nUseNewInput = false\r\nSteeringDeadZone = 0\r\n[DirectInput]\r\nUseDirectInputRemap = true\r\nBrakeAxis = 5\r\n"
        "AccelerationAxis = 2\r\nButtonGearUp = 13\r\nButtonGearDown = 12\r\nButtonStart = 35\r\nButtonSelUp = 8\r\n[WheelSettings]\r\n"
        "View = Simple\r\n[DirectInput.Calibration]\r\nBrakeCenter = 32767.5\r\nBrakeEnabled = true\r\nBrakeMaximum = 65535\r\nBrakeMinimum = 0\r\n"
        "[FFB]\r\nDirectInputFFB = true\r\n";
    for (int pass = 0; pass < 2; ++pass) {
        if (pass) spit(backup, owner);
        for (int fault = 1; fault <= 2; ++fault) {
            spit(user, owner);
            check(!Write(user, p, fault), "fault " + std::to_string(fault) + " fails");
            check(slurp(user) == owner, "fault " + std::to_string(fault) + " keeps the bytes");
            check(!fs::exists(tmp), "no temporary file left");
            check(fs::exists(backup) == (pass == 1), "backup only when it existed");
        }
    }
    fs::remove(backup);
    spit(user, owner);
    check(Write(user, p), "clean write");
    const std::string applied = slurp(user);
    check(slurp(backup) == owner, "backup holds the original bytes");
    check(applied.find("[Controls]\r\nUseNewInput = false\r\nSteeringDeadZone = 0\r\n[DirectInput]\r\n") == 0, "Tweaks' own [Controls] untouched, CRLF kept");
    check(applied.find("BrakeMinimum = 1743\r\n") != std::string::npos && applied.find("BrakeMinimum = 0") == std::string::npos, "brake minimum replaced in place");
    check(applied.find("ButtonGearUp = 13\r\n") != std::string::npos && applied.find("ButtonSelUp = 8\r\n") != std::string::npos, "owner keys outside the profile kept");
    check(applied.find("[WheelSettings]\r\nView = Simple\r\n") != std::string::npos && applied.find("[FFB]\r\nDirectInputFFB = true\r\n") != std::string::npos, "other sections kept");
    check(applied.find("[ControlsApplied]\r\nRevision = rev-1\r\n") != std::string::npos, "revision recorded");
    size_t first = applied.find("BrakeAxis = 5"), again = applied.find("BrakeAxis = 5", first + 1);
    check(first != std::string::npos && again == std::string::npos, "no duplicate keys");
    check(!Pending(user, p), "applied revision is not pending");
    check(Write(user, p) && slurp(user) == applied, "the same plan again changes nothing");

    // Startup apply from the game folder.
    spit(dir / ProfileFileName, "; written by Wheelkit\n[Controls]\n");
    {
        std::ofstream out(dir / ProfileFileName, std::ios::app | std::ios::binary);
        for (const std::string& l : profile()) out << l << "\n";
    }
    fs::remove(user); fs::remove(backup);
    spit(user, owner);
    bool did = false;
    auto log = ApplyAtStartup(dir, did);
    check(did && !log.empty() && log[0].find("revision rev-1 applied") != std::string::npos, "startup applies");
    const std::string once = slurp(user);
    log = ApplyAtStartup(dir, did);
    check(!did && log.empty() && slurp(user) == once, "second start: nothing pending");
    spit(dir / "OutRun2006Tweaks.ini", "[Controls]\nUseNewInput = true\n");
    spit(user, "[DirectInput]\nUseDirectInputRemap = true\n");
    log = ApplyAtStartup(dir, did);
    check(!did && !log.empty() && log[0].find("UseNewInput") != std::string::npos && slurp(user) == "[DirectInput]\nUseDirectInputRemap = true\n",
          "main INI UseNewInput=true refuses");
    fs::remove_all(dir, ec);

    std::printf("%s %d profile-controls checks\n", g_failed ? "FAIL" : "PASS", g_checks);
    return g_failed ? 1 : 0;
}
