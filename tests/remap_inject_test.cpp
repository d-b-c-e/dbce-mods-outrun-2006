// Offline checks for src/remap_inject.cpp (dev-only test injection for the DirectInput remap, STD-033 section 6).
// Built and run by tools/Test-RemapInject.ps1 (MSVC x86). No game, device or force: delivery goes into a plain
// DIJOYSTATE2 with a fixed range instead of a device. The signal mute is latched per process at its first use, so
// arming is checked in two processes: this one unmuted (refused), and "--muted <dir>" with DBCE_OUTRUN_SIGNAL_MUTE set.
#include "remap_inject.hpp"
#include "profile_controls.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static int g_checks, g_failed;
static void check(bool ok, const std::string& what)
{
    ++g_checks;
    if (!ok) { ++g_failed; std::printf("FAIL %s\n", what.c_str()); }
}

#define WHEEL_DEV "{11111111-2222-3333-4444-555555555555}"
#define SHIFTER_DEV "{66666666-7777-8888-9999-aaaaaaaaaaaa}"
#define WHEEL "dev=" WHEEL_DEV " prod={0006346e-0000-0000-0000-504944564944}"
static const GUID kWheel = { 0x11111111, 0x2222, 0x3333, { 0x44, 0x44, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55 } };
static const GUID kShifter = { 0x66666666, 0x7777, 0x8888, { 0x99, 0x99, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa } };

static std::vector<std::string> profile(const std::string& revision = "rev-1")
{
    return {
        "Schema = 1", "Profile = Test Rig", "Revision = " + revision, "Transmission = Sequential",
        "steer = axis 0 " WHEEL " range=0..65535 rest=33265 travel=-1 name=\"MOZA R12 Base\"",
        "throttle = axis 2 " WHEEL " range=0..65535 rest=0 travel=+1 name=\"MOZA R12 Base\"",
        "brake = axis 5 " WHEEL " range=0..65535 rest=1743 travel=+1 name=\"MOZA R12 Base\"",
        "confirm = button 31 " WHEEL " name=\"MOZA R12 Base\"",
        "navDown = hat 0 18000 " WHEEL " name=\"MOZA R12 Base\"",
    };
}

static DIJOYSTATE2 resting()
{
    DIJOYSTATE2 s{};
    s.lX = 33265; s.lZ = 0; s.lRz = 1743;
    for (auto& p : s.rgdwPOV) p = 0xFFFFFFFFul;
    return s;
}

static void spit(const fs::path& p, const std::string& text) { std::ofstream(p, std::ios::binary) << text; }

// A game folder whose user INI stages the profile request, as Wheelkit writes it.
static fs::path gameDir(const char* name, const std::string& revision)
{
    const fs::path dir = fs::temp_directory_path() / name;
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);
    spit(dir / "OutRun2006Tweaks.ini", "[Controls]\r\nUseNewInput = false\r\n[DirectInput]\r\nUseDirectInputRemap = true\r\n");
    std::string user = "[WheelkitProfile]\r\n";
    for (const std::string& l : profile(revision)) user += l + "\r\n";
    spit(dir / "OutRun2006Tweaks.user.ini", user);
    return dir;
}

static fs::path localAppData(const char* name)
{
    const fs::path base = fs::temp_directory_path() / name;
    std::error_code ec;
    fs::remove_all(base, ec);
    fs::create_directories(base / "dbce" / "outrun2006");
    _putenv_s("LOCALAPPDATA", base.string().c_str());
    return base;
}

static std::string session(long long fromNow)
{
    return "nonce=abcdef0123\nexpires=" + std::to_string((long long)std::time(nullptr) + fromNow) + "\n";
}

static bool contains(const std::vector<std::string>& log, const char* text)
{
    for (const auto& l : log) if (l.find(text) != std::string::npos) return true;
    return false;
}

// Second process: DBCE_OUTRUN_SIGNAL_MUTE is set by the runner, so the mute latches on and arming may proceed.
static int muted()
{
    const fs::path app = localAppData("outrun-remap-inject-appdata-muted");
    const fs::path dir = gameDir("outrun-remap-inject-muted", "rev-9");
    auto log = RemapInject::Init(dir);
    check(log.empty() && !RemapInject::Armed(), "no inject.on: nothing requested, nothing logged");
    spit(app / "dbce" / "outrun2006" / "inject.on", session(600));
    log = RemapInject::Init(dir);
    check(!RemapInject::Armed() && contains(log, "not applied yet"), "a pending profile request refuses");
    bool applied = false;
    ProfileControls::ApplyAtStartup(dir, applied);
    check(applied, "fixture: the request applies");
    log = RemapInject::Init(dir);
    check(RemapInject::Armed() && contains(log, "ARMED") && contains(log, "revision rev-9"), "applied + muted + session arms");
    spit(app / "dbce" / "outrun2006" / "inject.on", session(7200));
    log = RemapInject::Init(dir);
    check(!RemapInject::Armed() && contains(log, "more than an hour"), "a session beyond an hour refuses");
    spit(app / "dbce" / "outrun2006" / "inject.on", "unattended test run\n");
    log = RemapInject::Init(dir);
    check(!RemapInject::Armed() && contains(log, "nonce"), "an inject.on without a session refuses");
    std::printf("RemapInject (muted process): %d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}

int main(int argc, char** argv)
{
    if (argc > 1 && !std::strcmp(argv[1], "--muted")) return muted();

    // This process is not signal-muted: a request is refused, whatever else holds.
    {
        const fs::path app = localAppData("outrun-remap-inject-appdata");
        const fs::path dir = gameDir("outrun-remap-inject", "rev-1");
        bool applied = false;
        ProfileControls::ApplyAtStartup(dir, applied);
        spit(app / "dbce" / "outrun2006" / "inject.on", session(600));
        const auto log = RemapInject::Init(dir);
        check(!RemapInject::Armed() && contains(log, "not signal-muted"), "an unmuted process refuses even with everything else in place");
    }
    std::string why;
    check(!RemapInject::TestCommand("inject raw button 31 dev=" WHEEL_DEV " value=1 ms=500", why) && why.find("session") != std::string::npos,
          "not armed: commands refused");

    check(RemapInject::InstanceText(kWheel) == WHEEL_DEV, "instance text is the toolkit's normalized form");
    check(RemapInject::TestArm(profile()) && RemapInject::Armed(), "test arm");
    RemapInject::TestClock(1000);

    // Raw samples replace exactly their objects on their own instance, in the device's range.
    check(RemapInject::TestCommand("inject raw axis 0 dev=" WHEEL_DEV " value=65535 ms=500", why), "raw axis accepted: " + why);
    DIJOYSTATE2 s = resting();
    check(RemapInject::TestDeliver(kWheel, s, 128, 1, 0, 65535) == 1 && s.lX == 65535 && s.lZ == 0 && s.lRz == 1743, "steering axis replaced, others untouched");
    s = resting();
    check(RemapInject::TestDeliver(kShifter, s, 128, 1, 0, 65535) == 0 && s.lX == 33265, "another instance is untouched");
    s = resting();
    check(RemapInject::TestDeliver(kWheel, s, 128, 1, -32768, 32767) == 1 && s.lX == 32767, "the device's own range");
    check(RemapInject::TestCommand("inject raw button 31 dev=" WHEEL_DEV " value=1 ms=500", why), "raw button accepted");
    check(RemapInject::TestCommand("inject raw hat 0 18000 dev=" WHEEL_DEV " value=18000 ms=500", why), "raw hat accepted");
    s = resting();
    check(RemapInject::TestDeliver(kWheel, s, 128, 1, 0, 65535) == 3 && s.rgbButtons[31] == 0x80 && s.rgdwPOV[0] == 18000, "button and hat replaced");
    s = resting();
    check(RemapInject::TestDeliver(kWheel, s, 16, 0, 0, 65535) == 1 && s.rgbButtons[31] == 0 && s.rgdwPOV[0] == 0xFFFFFFFFul,
          "objects the device does not have are never delivered");
    check(RemapInject::TestCommand("inject action throttle 1 ms=500", why), "action resolved through the applied profile: " + why);
    s = resting();
    check(RemapInject::TestDeliver(kWheel, s, 128, 1, 0, 65535) == 4 && s.lZ == 65535, "throttle -> axis 2");
    RemapInject::TestClock(1600);
    s = resting();
    check(RemapInject::TestDeliver(kWheel, s, 128, 1, 0, 65535) == 0 && s.lX == 33265 && s.lZ == 0, "samples end on time; the device's values stand");

    // A failed read or a released slot ends the instance's samples.
    check(RemapInject::TestCommand("inject raw axis 2 dev=" WHEEL_DEV " value=65535 ms=5000", why), "long sample");
    RemapInject::DeviceGone(kShifter);
    s = resting();
    check(RemapInject::TestDeliver(kWheel, s, 128, 1, 0, 65535) == 1, "another instance's release leaves it running");
    RemapInject::DeviceGone(kWheel);
    s = resting();
    check(RemapInject::TestDeliver(kWheel, s, 128, 1, 0, 65535) == 0 && s.lZ == 0, "its own release drops it");

    // The command file: this session's nonce first, then at most 32 commands in at most 4096 bytes.
    check(RemapInject::TestFile("nonce=test\r\ninject raw button 31 dev=" WHEEL_DEV " value=1 ms=500\r\n", why) == 1 && why.empty(), "file read");
    check(RemapInject::TestFile("nonce=other1234\ninject raw button 31 dev=" WHEEL_DEV " value=1 ms=500\n", why) == 0 && why.find("nonce") != std::string::npos, "another session's file unread");
    check(RemapInject::TestFile("inject raw button 31 dev=" WHEEL_DEV " value=1 ms=500\n", why) == 0 && why.find("nonce") != std::string::npos, "a file without a nonce unread");
    {
        std::string big = "nonce=test\n";
        for (int i = 0; i < 33; ++i) big += "inject raw button 31 dev=" WHEEL_DEV " value=1 ms=100\n";
        check(RemapInject::TestFile(big, why) == 32 && why.find("32 commands") != std::string::npos, "32 commands per file");
        check(RemapInject::TestFile(std::string("nonce=test\n") + std::string(4097, ' '), why) == 0 && why.find("4096") != std::string::npos, "over 4096 bytes unread");
    }

    // The session ends at its expiry: samples dropped, injection off, commands refused.
    check(RemapInject::TestCommand("inject raw axis 0 dev=" WHEEL_DEV " value=0 ms=5000", why), "sample before expiry");
    RemapInject::TestExpire(2000);
    RemapInject::TestClock(2000);
    s = resting();
    check(RemapInject::TestDeliver(kWheel, s, 128, 1, 0, 65535) == 0 && !RemapInject::Armed() && s.lX == 33265, "expiry drops samples and disarms");
    check(!RemapInject::TestCommand("inject raw axis 0 dev=" WHEEL_DEV " value=0 ms=500", why), "no commands after expiry");

    // inject.on names the session.
    std::string out;
    check(RemapInject::TestSession("nonce=abcd1234\r\nexpires=1900\r\n", 1000, out) && out == "abcd1234", "session accepted");
    check(!RemapInject::TestSession("nonce=abcd1234\nexpires=1000\n", 1000, out) && out.find("expired") != std::string::npos, "expired");
    check(!RemapInject::TestSession("nonce=abcd1234\nexpires=4601\n", 1000, out) && out.find("hour") != std::string::npos, "beyond an hour");
    check(!RemapInject::TestSession("nonce=abc\nexpires=1900\n", 1000, out), "short nonce");
    check(!RemapInject::TestSession("nonce=abcd-1234\nexpires=1900\n", 1000, out), "non-alphanumeric nonce");
    check(!RemapInject::TestSession("nonce=abcd1234\n", 1000, out) && out.find("expires") != std::string::npos, "no expiry");

    std::printf("RemapInject: %d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
