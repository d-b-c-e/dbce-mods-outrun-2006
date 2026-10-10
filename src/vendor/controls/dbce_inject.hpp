// Test injection commands (STD-033 section 6, docs/controls-contract.md): development only, read by a mod from its
// gated dev file. Pure C++17; the DirectInput layer delivers the result as a raw device sample before the game's own
// binding resolution.
//   inject raw <axis|button|hat> <index> [angle] dev=<guid> value=<raw> ms=<50-15000> [range=<min>..<max>]
//   inject action <id> <n> ms=<50-15000>
// Raw axis values are in `range` (default 0..65535, Wheelkit's capture range); buttons take 1 (down) or 0; hats an angle or -1.
#pragma once
#include "dbce_controls.hpp"

namespace dbce { namespace controls {

struct InjectCommand {
    std::string action;      // "" for raw
    Binding binding;         // kind, index, angle, dev, range (min/max)
    long raw = 0;            // in binding.min..max (axes), 0/128 (buttons), angle or -1 (hats)
    int ms = 0;
};

inline bool parseInject(const std::string &lineIn, const Section *applied, InjectCommand &out, std::string &why)
{
    out = InjectCommand{};
    std::string line = trim(lineIn);
    std::vector<std::string> t;
    for (size_t i = 0; i < line.size();) {
        while (i < line.size() && line[i] == ' ') ++i;
        size_t j = line.find(' ', i);
        if (i < line.size()) t.push_back(line.substr(i, j == std::string::npos ? std::string::npos : j - i));
        i = j == std::string::npos ? line.size() : j;
    }
    auto ms = [&](const std::string &tok) {
        long v = 0;
        if (tok.compare(0, 3, "ms=") != 0 || !detail::integer(tok.substr(3), v) || v < 50 || v > 15000) return false;
        out.ms = (int)v;
        return true;
    };
    if (t.size() < 2 || t[0] != "inject") { why = "not an inject command"; return false; }
    if (t[1] == "action") {
        if (t.size() != 5) { why = "inject action <id> <n> ms=<50-15000>"; return false; }
        if (!applied) { why = "no applied [Controls]"; return false; }
        const Binding *b = find(*applied, t[2]);
        if (!b) { why = t[2] + " is not bound"; return false; }
        char *end = nullptr;
        double n = std::strtod(t[3].c_str(), &end);
        if (!end || *end || !std::isfinite(n)) { why = "bad value " + t[3]; return false; }
        if (!ms(t[4])) { why = "bad ms"; return false; }
        out.action = t[2];
        out.binding = *b;
        out.raw = denormalize(t[2], *b, n);
        return true;
    }
    if (t[1] != "raw") { why = "inject raw|action"; return false; }
    if (t.size() < 4) { why = "inject raw <kind> <index> ..."; return false; }
    Binding &b = out.binding;
    long v = 0;
    if (t[2] == "axis") b.kind = Kind::Axis; else if (t[2] == "button") b.kind = Kind::Button; else if (t[2] == "hat") b.kind = Kind::Hat;
    else { why = "bad kind"; return false; }
    if (!detail::integer(t[3], v) || v < 0 || v > (b.kind == Kind::Axis ? 7 : b.kind == Kind::Hat ? 3 : 127)) { why = "bad index"; return false; }
    b.index = (int)v;
    size_t k = 4;
    if (b.kind == Kind::Hat) {
        if (t.size() < 5 || !detail::integer(t[4], v) || v < 0 || v > 35999) { why = "bad angle"; return false; }
        b.angle = (int)v;
        k = 5;
    }
    b.min = 0; b.max = 65535;
    bool haveValue = false, haveMs = false;
    for (; k < t.size(); ++k) {
        const std::string &a = t[k];
        if (a.compare(0, 4, "dev=") == 0) { b.dev = detail::guid(a.substr(4)); if (b.dev.empty()) { why = "bad dev"; return false; } }
        else if (a.compare(0, 6, "value=") == 0) { if (!detail::integer(a.substr(6), out.raw)) { why = "bad value"; return false; } haveValue = true; }
        else if (a.compare(0, 6, "range=") == 0) {
            std::string r = a.substr(6);
            size_t d = r.find("..");
            long lo = 0, hi = 0;
            if (d == std::string::npos || !detail::integer(r.substr(0, d), lo) || !detail::integer(r.substr(d + 2), hi) || hi <= lo) { why = "bad range"; return false; }
            b.min = lo; b.max = hi;
        } else if (a.compare(0, 3, "ms=") == 0) { if (!ms(a)) { why = "bad ms"; return false; } haveMs = true; }
        else { why = "unknown " + a; return false; }
    }
    if (b.dev.empty()) { why = "dev= is required (the device instance)"; return false; }
    if (!haveValue || !haveMs) { why = "value= and ms= are required"; return false; }
    if (b.kind == Kind::Axis && (out.raw < b.min || out.raw > b.max)) { why = "value outside the range"; return false; }
    if (b.kind == Kind::Button && out.raw != 0 && out.raw != 1) { why = "a button takes 1 (down) or 0"; return false; }
    if (b.kind == Kind::Hat && out.raw != -1 && out.raw != b.angle) { why = "a hat takes its angle or -1"; return false; }
    return true;
}

}} // namespace dbce::controls
