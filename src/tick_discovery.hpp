#pragma once
// Read-only local-car tick discovery (docs/STAGE-PLAYBACK.md step 1). An external, expiring request arms one bounded
// window. Rows go to memory reserved at arming and are written once: when the window ends, fails, is stopped or the
// game exits normally. Discovery evidence only, never a replayable trajectory. Nothing here writes game memory or
// changes force, input, telemetry or display output. Game reads live in tick_discovery.cpp; this header has no game
// dependency so the x86 fixture (tools/tests/tick_discovery_fixture.cpp) exercises the same code.
#include <Windows.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace TickDiscovery {
inline constexpr char Schema[] = "outrun2006.tick-discovery@1";
inline constexpr int MinSeconds = 10, MaxSeconds = 120, UpdatesPerSecond = 60;
inline constexpr long long MaxLeadSeconds = 600; // a request must expire within ten minutes of being read
inline constexpr std::size_t MaxRequestBytes = 1024, Headroom = 1024;

struct Request { std::string id; int seconds = 0; long long expiresUnix = 0; };

inline bool LowerHex(std::string_view s, std::size_t n) {
    if (s.size() != n) return false;
    for (char c : s) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}
inline bool Integer(std::string_view s, long long min, long long max, long long& out) {
    if (s.empty() || s.size() > 18 || (s.size() > 1 && s[0] == '0')) return false;
    long long v = 0;
    for (char c : s) { if (c < '0' || c > '9') return false; v = v * 10 + (c - '0'); }
    if (v < min || v > max) return false;
    out = v; return true;
}

// Empty when accepted, otherwise the refusal. Exactly these five keys, once each; LF or CRLF lines.
inline std::string ParseRequest(std::string_view text, long long nowUnix, std::string_view gameSha256, Request& out) {
    if (text.size() > MaxRequestBytes) return "request too large";
    std::string action, id, seconds, expires, sha;
    for (std::size_t start = 0; start < text.size();) {
        std::size_t end = text.find('\n', start);
        if (end == std::string_view::npos) end = text.size();
        std::string_view line = text.substr(start, end - start);
        start = end + 1;
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.empty()) continue;
        const std::size_t eq = line.find('=');
        if (eq == std::string_view::npos) return "malformed line";
        const std::string_view key = line.substr(0, eq), value = line.substr(eq + 1);
        std::string* slot = key == "action" ? &action : key == "id" ? &id : key == "seconds" ? &seconds :
            key == "expiresUnix" ? &expires : key == "gameSha256" ? &sha : nullptr;
        if (!slot) return "unknown key";
        if (!slot->empty() || value.empty()) return "repeated or empty key";
        slot->assign(value);
    }
    long long s = 0, e = 0;
    if (action != "discover") return "action must be discover";
    if (!LowerHex(id, 32)) return "id must be 32 lowercase hex";
    if (!Integer(seconds, MinSeconds, MaxSeconds, s)) return "seconds out of range";
    if (!Integer(expires, 0, 999999999999999999LL, e)) return "malformed expiry";
    if (e <= nowUnix) return "request expired";
    if (e > nowUnix + MaxLeadSeconds) return "expiry too far ahead";
    if (sha != gameSha256) return "game hash mismatch";
    out = { id, int(s), e };
    return {};
}

enum class Phase { Pre, Post };
// Named values read around GamePlCar_Ctrl. `car` is used only for the pre/post identity check and is never written.
struct Observation {
    std::uintptr_t car = 0;
    bool playerCar = false, network = false;
    std::int64_t micros = 0; // since arming
    int ticks = 0, appTime = 0, powerOn = 0, currentMode = 0, gameMode = 0, stage = 0;
    int carId = 0, carKind = 0, carColour = 0, manual = 0;
    std::uint32_t flags = 0, gear = 0;
    int pedal = 0;
    float speed = 0;
    std::array<float, 3> position{}, velocity{};
    std::array<float, 16> m70{}, mB0{}, mF0{};
};
struct Row { Phase phase; std::uint32_t update; Observation o; };

inline bool Finite(const Observation& o) {
    if (!std::isfinite(o.speed)) return false;
    for (float v : o.position) if (!std::isfinite(v)) return false;
    for (float v : o.velocity) if (!std::isfinite(v)) return false;
    for (const auto* m : { &o.m70, &o.mB0, &o.mF0 }) for (float v : *m) if (!std::isfinite(v)) return false;
    return true;
}

// One bounded window. A failure keeps the rows recorded so far and stops recording; `ended` is a clean stop on a
// car or stage change, recorded after that row so the transition itself is visible.
class Session {
public:
    Request request;
    std::vector<Row> rows;
    std::size_t capacity = 0;
    std::uint32_t first = 0, current = 0, updates = 0, gameUpdates = 0, gameUpdatesWithoutPair = 0, maxPairs = 0,
        totalPairs = 0;
    std::string failure, ended;

    // Arming happens at the top of a real update, before that update's car tick, so the window opens it at once:
    // it covers updates first .. first + seconds x 60 - 1.
    bool Begin(const Request& r, std::uint32_t update, bool inGame) {
        request = r; first = current = update;
        capacity = std::size_t(r.seconds) * UpdatesPerSecond * 2 + Headroom;
        try { rows.reserve(capacity); } catch (...) { return false; }
        open = true; pairs = 0; currentInGame = inGame;
        return true;
    }
    bool Stopped() const { return !failure.empty() || !ended.empty(); }
    // True when `next` would be the first update after the window.
    bool Due(std::uint32_t next) const { return next - first >= std::uint32_t(request.seconds * UpdatesPerSecond); }
    // Start of each real game update (never a render-only iteration). Nothing opens after a failure or end.
    void Update(std::uint32_t update, bool inGame) {
        Close();
        if (pending) Fail("pre without post across updates");
        if (update != current + 1) Fail("update counter skipped or reordered");
        if (Stopped()) return;
        current = update; open = true; pairs = 0; currentInGame = inGame;
    }
    void Observe(Phase phase, std::uint32_t update, const Observation& o) {
        if (Stopped()) return;
        if (update != current || !open) return Fail("observation outside the current update");
        if (!o.playerCar) return Fail("car is not the local player (event 8)");
        if (o.network) return Fail("network driver or lobby active");
        if (!Finite(o)) return Fail("non-finite value");
        if (phase == Phase::Pre) {
            if (pending) return Fail("two pre observations");
            pending = true; preCar = o.car;
        } else {
            if (!pending) return Fail("post without pre");
            if (o.car != preCar) return Fail("post on a different car");
            pending = false; ++pairs; ++totalPairs;
        }
        if (rows.size() == capacity) return Fail("capacity");
        rows.push_back({ phase, update, o });
        const Observation& a = rows.front().o;
        if (o.carId != a.carId || o.carKind != a.carKind || o.carColour != a.carColour || o.manual != a.manual)
            ended = "car changed";
        else if (o.stage != a.stage) ended = "stage changed";
    }
    void Fail(std::string why) { if (failure.empty()) failure = std::move(why); }
    void Close() {
        if (!open) return;
        open = false;
        if (pairs > maxPairs) maxPairs = pairs;
        if (currentInGame) { ++gameUpdates; if (!pairs) ++gameUpdatesWithoutPair; }
        ++updates;
    }
    bool UnmatchedPre() const { return pending; }

private:
    bool open = false, pending = false, currentInGame = false;
    std::uintptr_t preCar = 0;
    std::uint32_t pairs = 0;
};

inline void Append(std::string& out, const char* format, double v) {
    char b[48]; std::snprintf(b, sizeof(b), format, v); out += b;
}
inline std::string Tsv(const Session& s) {
    std::string out = "phase\tupdate\tmicros\tticks\tapp_time\tpower_on_timer\tcurrent_mode\tgame_mode\tstage\tcar_id\t"
        "car_kind\tcar_colour\tmanual_transmission\tflags_4\tcur_gear_208\tpedal_amount_34\tfield_1c4\t"
        "position_x\tposition_y\tposition_z\tspd_mb_x\tspd_mb_y\tspd_mb_z";
    for (const char* m : { "matrix_70", "matrix_b0", "matrix_f0" })
        for (int r = 1; r <= 4; ++r) for (int c = 1; c <= 4; ++c) { out += '\t'; out += m; out += '_'; out += char('0' + r); out += char('0' + c); }
    out += '\n';
    out.reserve(out.size() + s.rows.size() * 700);
    for (const Row& row : s.rows) {
        const Observation& o = row.o;
        out += row.phase == Phase::Pre ? "pre" : "post";
        for (double v : { double(row.update), double(o.micros), double(o.ticks), double(o.appTime), double(o.powerOn),
                          double(o.currentMode), double(o.gameMode), double(o.stage), double(o.carId), double(o.carKind),
                          double(o.carColour), double(o.manual), double(o.flags), double(o.gear), double(o.pedal) })
            Append(out, "\t%.17g", v);
        Append(out, "\t%.9g", o.speed);
        for (float v : o.position) Append(out, "\t%.9g", v);
        for (float v : o.velocity) Append(out, "\t%.9g", v);
        for (const auto* m : { &o.m70, &o.mB0, &o.mF0 }) for (float v : *m) Append(out, "\t%.9g", v);
        out += '\n';
    }
    return out;
}
inline std::string OutcomeText(const Session& s, std::string_view outcome, std::string_view detail, std::string_view hooks) {
    std::string out;
    auto line = [&](std::string_view k, std::string_view v) { out.append(k); out += '='; out.append(v); out += '\n'; };
    auto number = [&](std::string_view k, unsigned long long v) { line(k, std::to_string(v)); };
    line("schema", Schema); line("id", s.request.id); line("outcome", outcome); line("detail", detail);
    number("seconds", unsigned(s.request.seconds)); number("rows", s.rows.size()); number("pairs", s.totalPairs);
    number("updates", s.updates); number("gameUpdates", s.gameUpdates);
    number("gameUpdatesWithoutPair", s.gameUpdatesWithoutPair); number("maxPairsPerUpdate", s.maxPairs);
    line("unmatchedPre", s.UnmatchedPre() ? "true" : "false"); line("hooks", hooks);
    line("evidence", "discovery"); line("replayable", "false"); line("writer", "none");
    line("output", "unchanged (no force, input, telemetry or display change)");
    return out;
}

inline bool WriteNew(const std::wstring& path, const std::string& bytes) {
    const std::wstring temp = path + L".tmp";
    HANDLE f = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    bool ok = true;
    for (std::size_t done = 0; ok && done < bytes.size();) {
        DWORD wrote = 0, chunk = DWORD((bytes.size() - done) > (1u << 20) ? (1u << 20) : (bytes.size() - done));
        ok = WriteFile(f, bytes.data() + done, chunk, &wrote, nullptr) && wrote == chunk;
        done += wrote;
    }
    ok = FlushFileBuffers(f) && ok;
    CloseHandle(f);
    if (ok) ok = MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH) != FALSE; // never replaces
    if (!ok) DeleteFileW(temp.c_str());
    return ok;
}
inline bool ReadSmall(const std::wstring& path, std::string& out) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    char b[MaxRequestBytes + 1]; DWORD read = 0;
    const bool ok = ReadFile(f, b, sizeof(b), &read, nullptr) != FALSE;
    CloseHandle(f);
    if (!ok) return false;
    out.assign(b, read);
    return true;
}
inline bool IsFile(const std::wstring& path) {
    const DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

// Game-thread controller. Every call returns a log line, empty when nothing happened.
class Controller {
public:
    std::wstring root; // ...\Dbce\StagePlayback\outrun-discovery
    std::string hooks;
    std::unique_ptr<Session> session;
    std::wstring dir;
    std::uint32_t update = 0;
    bool finalized = false;

    // `unavailable` is why this build cannot observe (unverified game, inactive car hook); empty when it can.
    std::string OnUpdate(bool inGame, bool network, long long nowUnix, std::string_view gameSha256, std::string_view unavailable) {
        ++update;
        if (finalized || root.empty()) return {};
        if (session) {
            // Decide before opening this update, so a closing window never counts an update it did not observe.
            const bool due = session->Due(update);
            const bool stop = !due && update % UpdatesPerSecond == 0 && IsFile(dir + L"\\stop.txt");
            if (network) session->Fail("network driver or lobby active");
            if (!due && !stop && !session->Stopped()) session->Update(update, inGame);
            if (!session->failure.empty()) return Write("failed", session->failure);
            if (!session->ended.empty()) return Write("ended", session->ended);
            if (due) return Write("observed", "duration ended");
            if (stop) return Write("stopped", "external stop");
            return {};
        }
        if (update % UpdatesPerSecond != 0) return {};
        return TryArm(nowUnix, gameSha256, unavailable, inGame);
    }
    void Observe(Phase phase, const Observation& o) { if (session && !finalized) session->Observe(phase, update, o); }
    // Normal exit at the outer-loop boundary. A failure or clean end seen in the last update keeps its own outcome.
    std::string Finalize() {
        if (finalized) return {};
        finalized = true;
        if (!session) return {};
        if (!session->failure.empty()) return Write("failed", session->failure);
        if (!session->ended.empty()) return Write("ended", session->ended);
        return Write("exit", "game exited before the window ended");
    }

private:
    std::string lastRefusal;
    std::string Refuse(const std::wstring& request, const std::string& why) {
        const bool moved = MoveFileExW(request.c_str(), (root + L"\\request.refused.txt").c_str(), MOVEFILE_REPLACE_EXISTING) != FALSE;
        if (!moved && why == lastRefusal) return {}; // a request that cannot be moved aside is refused again quietly
        lastRefusal = why;
        const std::wstring reason = root + L"\\refused.txt";
        DeleteFileW(reason.c_str());
        WriteNew(reason, "refused=" + why + "\n");
        return "TickDiscovery: request refused (" + why + ")" + (moved ? "" : "; request left in place");
    }
    std::string TryArm(long long nowUnix, std::string_view gameSha256, std::string_view unavailable, bool inGame) {
        const std::wstring request = root + L"\\request.txt";
        if (!IsFile(request)) return {};
        std::string text; Request r;
        if (!ReadSmall(request, text)) return Refuse(request, "unreadable request");
        std::string why = unavailable.empty() ? ParseRequest(text, nowUnix, gameSha256, r) : std::string(unavailable);
        if (!why.empty()) return Refuse(request, why);
        const std::wstring next = root + L"\\" + std::wstring(r.id.begin(), r.id.end());
        if (!CreateDirectoryW(next.c_str(), nullptr)) return Refuse(request, "result folder exists or cannot be created");
        if (!MoveFileExW(request.c_str(), (next + L"\\request.txt").c_str(), 0)) return Refuse(request, "request could not be claimed");
        auto s = std::unique_ptr<Session>(new (std::nothrow) Session);
        if (!s || !s->Begin(r, update, inGame)) {
            WriteNew(next + L"\\outcome.txt", std::string("schema=") + Schema + "\nid=" + r.id + "\noutcome=failed\ndetail=memory\n");
            return "TickDiscovery: could not reserve memory";
        }
        session = std::move(s); dir = next;
        return "TickDiscovery: armed " + r.id + " for " + std::to_string(r.seconds) + " s";
    }
    std::string Write(std::string_view outcome, std::string_view detail) {
        session->Close();
        const bool rows = WriteNew(dir + L"\\discovery.tsv", Tsv(*session));
        const bool done = WriteNew(dir + L"\\outcome.txt", OutcomeText(*session, outcome, detail, hooks)); // written last
        std::string log = "TickDiscovery: " + session->request.id + " " + std::string(outcome) + " (" + std::string(detail) +
            "), " + std::to_string(session->rows.size()) + " rows" + (rows && done ? "" : "; WRITE FAILED");
        session.reset(); dir.clear();
        return log;
    }
};
} // namespace TickDiscovery
