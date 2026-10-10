// Rig-profile controls -> the DirectInput remap's keys (see profile_controls.hpp). The [Controls] parser is the
// toolkit's (src/vendor/controls/dbce_controls.hpp, toolkit 65c686d); this file holds only the OutRun 2006 mapping and
// the user INI edit. The remap reads DIJOYSTATE2 itself, so the profile's DirectInput indexes and instance GUIDs carry
// over unchanged (hooks_inputremap.cpp ReadAxisRaw: 0 lX .. 5 lRz, 6-7 sliders; buttons 0..127).
#include "profile_controls.hpp"

#include "pov_binding.hpp"
#include "vendor/controls/dbce_controls.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace ctl = dbce::controls;

namespace ProfileControls
{
namespace
{
constexpr const char* Remap = "DirectInput";
constexpr const char* Calibration = "DirectInput.Calibration";
constexpr const char* Shifter = "DirectInput.Shifter";
constexpr const char* Aux = "DirectInput.Aux";

// Menu and camera actions: the same key name in the primary [DirectInput] and the [DirectInput.Aux] slot.
struct Digital { const char* action; const char* key; };
constexpr Digital kDigital[] = {
    {"confirm", "ButtonA"}, {"back", "ButtonB"}, {"start", "ButtonStart"}, {"select", "ButtonBack"},
    {"camera", "ButtonChangeView"}, {"navUp", "ButtonSelUp"}, {"navDown", "ButtonSelDown"},
    {"navLeft", "ButtonSelLeft"}, {"navRight", "ButtonSelRight"},
};
constexpr const char* kGears[] = {"gear1", "gear2", "gear3", "gear4", "gear5", "gear6"};

bool iequal(const std::string& a, const std::string& b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (ctl::detail::lower(a[i]) != ctl::detail::lower(b[i])) return false;
    return true;
}

// The owner's INIs write GUIDs in upper case with braces; the remap parses either case.
std::string guidText(const std::string& g)
{
    std::string s = g;
    for (char& c : s) if (c >= 'a' && c <= 'f') c = char(c - 'a' + 'A');
    return s;
}

std::string number(long v) { return std::to_string(v); }

struct Builder
{
    Plan& p;
    std::string shifterDev, auxDev;
    void key(const char* section, const char* k, const std::string& v)
    {
        for (Key& e : p.keys)
            if (e.section == section && e.key == k) { e.value = v; return; }
        p.keys.push_back({section, k, v});
    }
    void note(const std::string& action, const std::string& why) { p.notes.push_back(action + ": " + why); }
    // One device per optional slot: the first binding chooses it, a binding on a third device is not applied.
    bool slot(std::string& dev, const char* section, const ctl::Binding& b, const std::string& action)
    {
        if (dev.empty()) { dev = b.dev; key(section, "DeviceGuid", guidText(b.dev)); return true; }
        if (dev == b.dev) return true;
        note(action, std::string("the remap's ") + section + " slot already holds another device");
        return false;
    }
};

std::string sectionName(const std::string& line)
{
    size_t a = line.find_first_not_of(" \t");
    if (a == std::string::npos || line[a] != '[') return std::string();
    size_t b = line.find(']', a);
    return b == std::string::npos ? std::string() : line.substr(a + 1, b - a - 1);
}

std::string lineKey(const std::string& line)
{
    std::string t = ctl::trim(line);
    if (t.empty() || t[0] == ';' || t[0] == '#' || t[0] == '[') return std::string();
    size_t eq = t.find('=');
    return eq == std::string::npos ? std::string() : ctl::trim(t.substr(0, eq));
}

struct Doc { std::vector<std::string> lines; bool crlf = false; };

bool load(const std::filesystem::path& path, Doc& d)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();
    d.crlf = text.find("\r\n") != std::string::npos;
    for (size_t start = 0; start < text.size();) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        d.lines.push_back(line);
        start = end + 1;
    }
    return true;
}

std::string render(const Doc& d)
{
    std::string out;
    for (const std::string& l : d.lines) out += l + (d.crlf ? "\r\n" : "\n");
    return out;
}

void set(Doc& d, const std::string& section, const std::string& key, const std::string& value)
{
    auto& lines = d.lines;
    const std::string assign = key + " = " + value;
    int start = -1, end = (int)lines.size();
    for (int i = 0; i < (int)lines.size(); ++i) {
        if (sectionName(lines[i]).empty()) continue;
        if (start >= 0) { end = i; break; }
        if (iequal(sectionName(lines[i]), section)) start = i + 1;
    }
    if (start < 0) {
        if (!lines.empty() && !ctl::trim(lines.back()).empty()) lines.push_back("");
        lines.push_back("[" + section + "]");
        lines.push_back(assign);
        return;
    }
    for (int i = start; i < end; ++i)
        if (iequal(lineKey(lines[i]), key)) { lines[i] = assign; return; }
    int at = end;
    while (at > start && ctl::trim(lines[at - 1]).empty()) --at;
    lines.insert(lines.begin() + at, assign);
}

std::string value(const Doc& d, const std::string& section, const std::string& key)
{
    bool in = false;
    for (const std::string& l : d.lines) {
        if (!sectionName(l).empty()) { in = iequal(sectionName(l), section); continue; }
        if (in && iequal(lineKey(l), key)) return ctl::trim(l.substr(l.find('=') + 1));
    }
    return std::string();
}

bool replace(const std::filesystem::path& path, const std::string& text, int fault)
{
    std::filesystem::path tmp = path;
    tmp += ".controls-tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        if (fault == 1) {
            out << text.substr(0, text.size() / 2);
            out.close();
            std::error_code ec;
            std::filesystem::remove(tmp, ec);
            return false;
        }
        out << text;
        out.flush();
        if (!out) { out.close(); std::error_code ec; std::filesystem::remove(tmp, ec); return false; }
    }
    const bool moved = fault != 2 &&
        MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!moved) { std::error_code ec; std::filesystem::remove(tmp, ec); }
    return moved;
}

uint32_t fnv1a(const std::string& s)
{
    uint32_t h = 2166136261u;
    for (unsigned char c : s) { h ^= c; h *= 16777619u; }
    return h;
}
} // namespace

Plan PlanLines(const std::vector<std::string>& lines, bool useNewInput)
{
    Plan p;
    std::string joined;
    for (const std::string& l : lines) joined += l + "\n";
    ctl::Section s = ctl::parseSection(lines);
    p.profile = s.profile;
    if (!s.revision.empty()) p.revision = s.revision;
    else { char h[16]; std::snprintf(h, sizeof(h), "%08x", (unsigned)fnv1a(joined)); p.revision = std::string("fnv1a:") + h; }
    if (!s.error.empty()) { p.error = s.error; return p; }
    if (useNewInput) { p.error = "UseNewInput is on, so the DirectInput remap is inactive; the input backend is left as it is"; return p; }
    Builder b{p};
    for (auto& bad : s.invalid) b.note(bad.first, bad.second);

    const ctl::Binding* steer = ctl::find(s, "steer");
    if (!steer) { p.error = "no steer binding: the remap's primary device is the steering wheel"; return p; }
    auto fullRange = [](const ctl::Binding& x) { return x.min == 0 && x.max == 65535; };
    if (!fullRange(*steer)) { p.error = "steering range is not 0..65535 (the remap's axis range)"; return p; }
    const std::string primary = steer->dev;
    b.key(Remap, "UseDirectInputRemap", "true");
    b.key(Remap, "DeviceGuid", guidText(steer->dev));
    b.key(Remap, "SteeringAxis", number(steer->index));
    b.key(Remap, "SteeringInvert", steer->inverted ? "true" : "false");
    // The contract centres steering on the range, as the remap's uncalibrated path does ((raw - 32767.5) / 32767.5).
    b.key(Calibration, "SteeringEnabled", "false");

    for (const ctl::Entry& e : s.bound) {
        const ctl::Binding& x = e.binding;
        if (e.action == "steer") continue;
        if (e.action == "throttle" || e.action == "brake") {
            const bool gas = e.action == "throttle";
            if (x.kind != ctl::Kind::Axis) { b.note(e.action, "a button pedal has no remap setting (pedals are axes)"); continue; }
            if (!fullRange(x)) { b.note(e.action, "range is not 0..65535 (the remap's axis range)"); continue; }
            // Calibrated pedal: (raw - minimum) / (maximum - minimum), then 1 - n when inverted. With rest as the
            // released end this is the contract's normalization exactly, for either travel and the inverted flag.
            const long lo = x.travel > 0 ? x.rest : x.min, hi = x.travel > 0 ? x.max : x.rest;
            if (hi - lo < 8192) { b.note(e.action, "travel from rest is under 8192 (the remap refuses such a calibration)"); continue; }
            const bool invert = (x.travel < 0) != x.inverted;
            b.key(Remap, gas ? "AccelerationAxis" : "BrakeAxis", number(x.index));
            b.key(Remap, gas ? "AccelerationInvert" : "BrakeInvert", invert ? "true" : "false");
            b.key(Remap, gas ? "ThrottleDeviceGuid" : "BrakeDeviceGuid", guidText(x.dev));
            if (!x.name.empty()) b.key(Remap, gas ? "ThrottleDeviceName" : "BrakeDeviceName", x.name);
            const char* role = gas ? "Throttle" : "Brake";
            b.key(Calibration, (std::string(role) + "Enabled").c_str(), "true");
            b.key(Calibration, (std::string(role) + "Minimum").c_str(), number(lo));
            b.key(Calibration, (std::string(role) + "Center").c_str(), number((lo + hi) / 2));
            b.key(Calibration, (std::string(role) + "Maximum").c_str(), number(hi));
            continue;
        }
        if (e.action == "shiftUp" || e.action == "shiftDown") {
            const char* k = e.action == "shiftUp" ? "ButtonGearUp" : "ButtonGearDown";
            if (x.kind != ctl::Kind::Button) { b.note(e.action, "the remap binds buttons only"); continue; }
            if (x.dev == primary) b.key(Remap, k, number(x.index));
            else if (b.slot(b.shifterDev, Shifter, x, e.action)) b.key(Shifter, k, number(x.index));
            continue;
        }
        int gear = 0;
        for (int i = 0; i < 6; ++i) if (e.action == kGears[i]) gear = i + 1;
        if (gear || e.action == "reverse") {
            if (x.kind != ctl::Kind::Button) { b.note(e.action, "the remap binds buttons only"); continue; }
            if (!b.slot(b.shifterDev, Shifter, x, e.action)) continue;
            b.key(Shifter, gear ? ("ButtonGear" + std::to_string(gear)).c_str() : "ButtonGearReverse", number(x.index));
            continue;
        }
        const Digital* map = nullptr;
        for (const Digital& d : kDigital) if (e.action == d.action) map = &d;
        if (!map) { b.note(e.action, "no OutRun 2006 control"); continue; }
        long raw = x.index;
        if (x.kind == ctl::Kind::Hat) {   // pov_binding.hpp: 128 + hat * 4 + direction, straight directions only
            raw = PovBinding::Encode(x.index, x.angle);
            if (raw < 0) { b.note(e.action, "a diagonal hat direction has no remap binding"); continue; }
        }
        if (x.dev == primary) b.key(Remap, map->key, number(raw));
        else if (b.slot(b.auxDev, Aux, x, e.action)) b.key(Aux, map->key, number(raw));
    }
    // The shifter slot's mode follows the profile's transmission; an automatic profile leaves the mode alone.
    if (!b.shifterDev.empty()) {
        if (iequal(s.transmission, "Sequential")) b.key(Shifter, "GearMode", "sequential");
        else if (iequal(s.transmission, "HPattern") || iequal(s.transmission, "H-Pattern")) b.key(Shifter, "GearMode", "hpattern");
        else b.note("transmission", "'" + s.transmission + "' has no shifter mode; GearMode left as it is");
    }
    for (const std::string& a : s.unbound) b.note(a, "unbound in the profile; the remap keeps its value");
    p.ok = !p.keys.empty();
    if (!p.ok) p.error = "no binding the remap can use";
    return p;
}

bool ReadProfile(const std::filesystem::path& userIni, std::vector<std::string>& body)
{
    Doc d;
    if (!load(userIni, d)) return false;
    bool in = false, found = false;
    for (const std::string& l : d.lines) {
        if (!sectionName(l).empty()) { in = iequal(sectionName(l), ProfileSection); found = found || in; continue; }
        if (in) body.push_back(l);
    }
    return found;
}

bool UseNewInput(const std::filesystem::path& mainIni, const std::filesystem::path& userIni)
{
    std::string v;
    Doc u, m;
    if (load(userIni, u)) v = value(u, "Controls", "UseNewInput");
    if (v.empty() && load(mainIni, m)) v = value(m, "Controls", "UseNewInput");
    return iequal(v, "true") || v == "1";
}

bool Pending(const std::filesystem::path& userIni, const Plan& plan)
{
    Doc d;
    return !load(userIni, d) || value(d, "ControlsApplied", "Revision") != plan.revision;
}

bool Write(const std::filesystem::path& userIni, const Plan& plan, int fault)
{
    if (!plan.ok) return false;
    Doc d;
    load(userIni, d); // a missing user INI starts empty (the game then reads only these keys from it)
    const std::string original = render(d);
    std::filesystem::path backup = userIni;
    backup += ".before-profile-controls";
    std::error_code ec;
    if (!std::filesystem::exists(backup, ec) && std::filesystem::exists(userIni, ec)) {
        std::ifstream in(userIni, std::ios::binary);
        std::stringstream bytes;
        bytes << in.rdbuf();
        if (!in || !replace(backup, bytes.str(), fault)) return false;
    }
    for (const Key& k : plan.keys) set(d, k.section, k.key, k.value);
    set(d, "ControlsApplied", "Revision", plan.revision);
    set(d, "ControlsApplied", "Profile", plan.profile);
    for (const Key& k : plan.keys)
        if (value(d, k.section, k.key) != k.value) return false;
    if (value(d, "ControlsApplied", "Revision") != plan.revision) return false;
    const std::string text = render(d);
    return text == original || replace(userIni, text, fault);
}

std::vector<std::string> ApplyAtStartup(const std::filesystem::path& gameDir, bool& applied)
{
    applied = false;
    std::vector<std::string> log, body;
    const auto userIni = gameDir / "OutRun2006Tweaks.user.ini", mainIni = gameDir / "OutRun2006Tweaks.ini";
    if (!ReadProfile(userIni, body)) return log;
    Plan plan = PlanLines(body, UseNewInput(mainIni, userIni));
    if (!plan.ok) { log.push_back("ProfileControls: profile '" + plan.profile + "' not applied: " + plan.error); return log; }
    if (!Pending(userIni, plan)) return log;
    if (!Write(userIni, plan)) {
        log.push_back("ProfileControls: profile '" + plan.profile + "' revision " + plan.revision + ": could not write the user INI; nothing changed");
        return log;
    }
    applied = true;
    log.push_back("ProfileControls: profile '" + plan.profile + "' revision " + plan.revision + " applied: " +
        std::to_string(plan.keys.size()) + " keys in OutRun2006Tweaks.user.ini");
    for (const Key& k : plan.keys) log.push_back("ProfileControls:   [" + k.section + "] " + k.key + " = " + k.value);
    for (const std::string& n : plan.notes) log.push_back("ProfileControls:   not applied: " + n);
    return log;
}
}
