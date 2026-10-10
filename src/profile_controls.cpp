// Rig-profile controls -> the DirectInput remap's keys (see profile_controls.hpp). The binding parser is the toolkit's
// (src/vendor/controls/dbce_controls.hpp, toolkit 65c686d); every read of what the game will see goes through the
// game's own inih INIReader. The remap reads DIJOYSTATE2 itself, so the profile's DirectInput indexes and instance
// GUIDs carry over unchanged (hooks_inputremap.cpp ReadAxisRaw: 0 lX .. 5 lRz, 6-7 sliders; buttons 0..127; POV
// directions per pov_binding.hpp).
#include "profile_controls.hpp"

#include "pov_binding.hpp"
#include "vendor/controls/dbce_controls.hpp"

#include <ini.h>

#include <cstdio>
#include <fstream>
#include <set>
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
constexpr const char* Applied = "ControlsApplied";

// Menu and camera actions: the same key name in the primary [DirectInput] and the [DirectInput.Aux] slot.
struct Digital { const char* action; const char* key; };
constexpr Digital kDigital[] = {
    {"confirm", "ButtonA"}, {"back", "ButtonB"}, {"start", "ButtonStart"}, {"select", "ButtonBack"},
    {"camera", "ButtonChangeView"}, {"navUp", "ButtonSelUp"}, {"navDown", "ButtonSelDown"},
    {"navLeft", "ButtonSelLeft"}, {"navRight", "ButtonSelRight"},
};
constexpr const char* kGears[] = {"gear1", "gear2", "gear3", "gear4", "gear5", "gear6"};
// Every button key of a slot (cleared when the slot changes device, so no number from the old device stays live).
constexpr const char* kSlotButtons[] = {"ButtonA", "ButtonB", "ButtonX", "ButtonY", "ButtonStart", "ButtonBack",
    "ButtonGearUp", "ButtonGearDown", "ButtonChangeView", "ButtonSelUp", "ButtonSelDown", "ButtonSelLeft", "ButtonSelRight"};
constexpr const char* kShifterButtons[] = {"ButtonGearUp", "ButtonGearDown", "ButtonGear1", "ButtonGear2", "ButtonGear3",
    "ButtonGear4", "ButtonGear5", "ButtonGear6", "ButtonGearReverse"};

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
    bool has(const char* section, const char* k) const
    {
        for (const Key& e : p.keys) if (e.section == section && e.key == k) return true;
        return false;
    }
    void key(const char* section, const std::string& k, const std::string& v)
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
    // A slot given a different device keeps no button number from the old one.
    template <size_t N>
    void clearOnChange(const char* section, const std::string& planned, const std::string& current, bool unsetIsChange,
        const char* const (&keys)[N])
    {
        if (planned.empty()) return;
        const bool changed = current.empty() ? unsetIsChange : !iequal(current, guidText(planned));
        if (!changed) return;
        for (const char* k : keys)
            if (!has(section, k)) {
                key(section, k, "-1");
                note(std::string(section) + " " + k, "cleared (it belonged to the slot's previous device)");
            }
    }
};

// ---- the user INI as text, with the game's exact section and key spelling ------------------------------------------
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

struct Doc { std::vector<std::string> lines; bool crlf = false, bom = false; };  // a UTF-8 BOM is kept apart, as inih strips it

bool load(const std::filesystem::path& path, Doc& d)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    std::string text = ss.str();
    d.bom = text.compare(0, 3, "\xEF\xBB\xBF") == 0;
    if (d.bom) text.erase(0, 3);
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
    std::string out = d.bom ? "\xEF\xBB\xBF" : "";
    for (const std::string& l : d.lines) out += l + (d.crlf ? "\r\n" : "\n");
    return out;
}

// Exact spelling, as INIReader::Get matches: [directinput] is another section to the game.
void set(Doc& d, const std::string& section, const std::string& key, const std::string& value)
{
    auto& lines = d.lines;
    const std::string assign = key + " = " + value;
    int start = -1, end = (int)lines.size();
    for (int i = 0; i < (int)lines.size(); ++i) {
        if (sectionName(lines[i]).empty()) continue;
        if (start >= 0) { end = i; break; }
        if (sectionName(lines[i]) == section) start = i + 1;
    }
    if (start < 0) {
        if (!lines.empty() && !ctl::trim(lines.back()).empty()) lines.push_back("");
        lines.push_back("[" + section + "]");
        lines.push_back(assign);
        return;
    }
    for (int i = start; i < end; ++i)
        if (lineKey(lines[i]) == key) { lines[i] = assign; return; }
    int at = end;
    while (at > start && ctl::trim(lines[at - 1]).empty()) --at;
    lines.insert(lines.begin() + at, assign);
}

bool writeFile(const std::filesystem::path& path, const std::string& text, bool half)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << (half ? text.substr(0, text.size() / 2) : text);
    out.flush();
    return (bool)out && !half;
}

void drop(const std::filesystem::path& path) { std::error_code ec; std::filesystem::remove(path, ec); }

bool moveOver(const std::filesystem::path& from, const std::filesystem::path& to)
{
    return MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

uint32_t fnv1a(const std::string& s)
{
    uint32_t h = 2166136261u;
    for (unsigned char c : s) { h ^= c; h *= 16777619u; }
    return h;
}

template <typename T>
T effective(const inih::INIReader& ini, const char* section, const char* key, T current)
{
    return ini.Get<T>(section, key, current);   // an invalid or missing value keeps the previous one
}
} // namespace

Plan PlanLines(const std::vector<std::string>& lines, const Current& current)
{
    Plan p;
    std::string joined;
    for (const std::string& l : lines) joined += l + "\n";
    ctl::Section s = ctl::parseSection(lines);
    p.profile = s.profile;
    if (!s.revision.empty()) p.revision = s.revision;
    else { char h[16]; std::snprintf(h, sizeof(h), "%08x", (unsigned)fnv1a(joined)); p.revision = std::string("fnv1a:") + h; }
    if (!s.error.empty()) { p.error = s.error; return p; }
    if (current.useNewInput) { p.error = "UseNewInput is on, so the DirectInput remap is inactive; the input backend is left as it is"; return p; }
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

    std::set<long> gearButtons;
    bool gearClash = false;
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
            const std::string role = gas ? "Throttle" : "Brake";
            b.key(Calibration, role + "Enabled", "true");
            b.key(Calibration, role + "Minimum", number(lo));
            b.key(Calibration, role + "Center", number((lo + hi) / 2));
            b.key(Calibration, role + "Maximum", number(hi));
            continue;
        }
        long raw = x.index;
        if (x.kind == ctl::Kind::Hat) {   // pov_binding.hpp: 128 + hat * 4 + direction, straight directions only
            raw = PovBinding::Encode(x.index, x.angle);
            if (raw < 0) { b.note(e.action, "a diagonal hat direction has no remap binding"); continue; }
        }
        if (e.action == "shiftUp" || e.action == "shiftDown") {
            const char* k = e.action == "shiftUp" ? "ButtonGearUp" : "ButtonGearDown";
            if (x.dev == primary) b.key(Remap, k, number(raw));
            else if (b.slot(b.shifterDev, Shifter, x, e.action)) b.key(Shifter, k, number(raw));
            continue;
        }
        int gear = 0;
        for (int i = 0; i < 6; ++i) if (e.action == kGears[i]) gear = i + 1;
        if (gear || e.action == "reverse") {
            // The H-pattern reader (hooks_inputremap.cpp UpdateHPattern) reads rgbButtons only: a POV gear never selects.
            if (x.kind != ctl::Kind::Button) { b.note(e.action, "the H-pattern reader takes buttons only"); continue; }
            if (!b.slot(b.shifterDev, Shifter, x, e.action)) continue;
            gearClash = gearClash || !gearButtons.insert(raw).second;
            b.key(Shifter, gear ? "ButtonGear" + std::to_string(gear) : std::string("ButtonGearReverse"), number(raw));
            continue;
        }
        const Digital* map = nullptr;
        for (const Digital& d : kDigital) if (e.action == d.action) map = &d;
        if (!map) { b.note(e.action, "no OutRun 2006 control"); continue; }
        if (x.dev == primary) b.key(Remap, map->key, number(raw));
        else if (b.slot(b.auxDev, Aux, x, e.action)) b.key(Aux, map->key, number(raw));
    }
    // GearMode is always written, so an earlier H-pattern never outlives a profile that does not ask for one. H-pattern
    // only when this profile gives gear 1 and distinct gear buttons on the shifter slot; otherwise sequential.
    const bool hPattern = iequal(s.transmission, "HPattern") || iequal(s.transmission, "H-pattern");
    if (hPattern && b.has(Shifter, "ButtonGear1") && !gearClash) b.key(Shifter, "GearMode", "hpattern");
    else {
        b.key(Shifter, "GearMode", "sequential");
        if (hPattern) b.note("transmission", "H-pattern needs gear 1 and distinct gear buttons on one shifter; sequential written");
        else if (!iequal(s.transmission, "Sequential"))
            b.note("transmission", "'" + s.transmission + "': the race gearbox is chosen in the game; the shifter slot is set to sequential");
    }
    b.clearOnChange(Remap, primary, current.primaryDev, false, kSlotButtons);
    b.clearOnChange(Aux, b.auxDev, current.auxDev, true, kSlotButtons);
    b.clearOnChange(Shifter, b.shifterDev, current.shifterDev, true, kShifterButtons);
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
        if (!sectionName(l).empty()) { in = sectionName(l) == ProfileSection; found = found || in; continue; }
        if (in) body.push_back(l);
    }
    return found;
}

Current ReadCurrent(const std::filesystem::path& mainIni, const std::filesystem::path& userIni)
{
    Current c;
    for (const auto& path : {mainIni, userIni}) {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) continue;
        try {
            inih::INIReader ini(path);
            c.useNewInput = effective<bool>(ini, "Controls", "UseNewInput", c.useNewInput);
            c.primaryDev = effective<std::string>(ini, Remap, "DeviceGuid", c.primaryDev);
            c.shifterDev = effective<std::string>(ini, Shifter, "DeviceGuid", c.shifterDev);
            c.auxDev = effective<std::string>(ini, Aux, "DeviceGuid", c.auxDev);
        } catch (...) {
            // Settings::read skips a file it cannot parse; so does this.
        }
    }
    return c;
}

bool Pending(const std::filesystem::path& userIni, const Plan& plan)
{
    std::error_code ec;
    if (!std::filesystem::exists(userIni, ec)) return true;
    try {
        inih::INIReader ini(userIni);
        std::string none;
        return ini.Get<std::string>(Applied, "Revision", none) != plan.revision;
    } catch (...) {
        return true;
    }
}

bool Write(const std::filesystem::path& userIni, const Plan& plan, std::string& why, int fault)
{
    why.clear();
    if (!plan.ok) { why = "no plan"; return false; }
    Doc d;
    std::error_code ec;
    const bool existed = std::filesystem::exists(userIni, ec);
    if (existed && !load(userIni, d)) { why = "cannot read the user INI"; return false; }
    // An owned section under another letter case is a different section to the game: refuse rather than guess.
    std::set<std::string> owned = {Applied};
    for (const Key& k : plan.keys) owned.insert(k.section);
    for (const std::string& l : d.lines) {
        const std::string name = sectionName(l);
        for (const std::string& o : owned)
            if (!name.empty() && name != o && iequal(name, o)) { why = "[" + name + "] differs from [" + o + "] only in letter case"; return false; }
    }
    const std::string original = render(d);
    std::filesystem::path backup = userIni, tmp = userIni;
    backup += ".before-profile-controls";
    tmp += ".controls-tmp";
    if (existed && !std::filesystem::exists(backup, ec)) {
        std::filesystem::path backupTmp = backup;
        backupTmp += ".tmp";
        std::ifstream in(userIni, std::ios::binary);
        std::stringstream bytes;
        bytes << in.rdbuf();
        if (!in || !writeFile(backupTmp, bytes.str(), fault == 1) || fault == 2 || !moveOver(backupTmp, backup)) {
            drop(backupTmp);
            why = "cannot write the backup";
            return false;
        }
    }
    for (const Key& k : plan.keys) set(d, k.section, k.key, k.value);
    set(d, Applied, "Revision", plan.revision);
    set(d, Applied, "Profile", plan.profile);
    const std::string text = render(d);
    if (text == original) return true;
    if (!writeFile(tmp, text, fault == 1)) { drop(tmp); why = "cannot write the temporary file"; return false; }
    // Read back with the game's parser: every planned key must come out exactly as planned.
    try {
        inih::INIReader ini(tmp);
        for (const Key& k : plan.keys)
            if (ini.Get<std::string>(k.section, k.key) != k.value) throw std::runtime_error(k.section + " " + k.key);
        if (ini.Get<std::string>(Applied, "Revision") != plan.revision) throw std::runtime_error("revision");
    } catch (const std::exception& e) {
        drop(tmp);
        why = std::string("read-back differs: ") + e.what();
        return false;
    }
    if (fault == 2 || !moveOver(tmp, userIni)) { drop(tmp); why = "cannot replace the user INI"; return false; }
    return true;
}

std::vector<std::string> ApplyAtStartup(const std::filesystem::path& gameDir, bool& applied)
{
    applied = false;
    std::vector<std::string> log, body;
    const auto userIni = gameDir / "OutRun2006Tweaks.user.ini", mainIni = gameDir / "OutRun2006Tweaks.ini";
    if (!ReadProfile(userIni, body)) return log;
    Plan plan = PlanLines(body, ReadCurrent(mainIni, userIni));
    if (!plan.ok) { log.push_back("ProfileControls: profile '" + plan.profile + "' not applied: " + plan.error); return log; }
    if (!Pending(userIni, plan)) return log;
    std::string why;
    if (!Write(userIni, plan, why)) {
        log.push_back("ProfileControls: profile '" + plan.profile + "' revision " + plan.revision + " not applied: " + why + "; nothing changed");
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
