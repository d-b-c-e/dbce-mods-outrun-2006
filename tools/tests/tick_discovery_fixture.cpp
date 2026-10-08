// Production tick_discovery.hpp against a temporary root: request parsing, window ordering/bounds and the exact
// files written. No game, no hook, no device. Usage: tick-discovery.exe <empty temp folder>
#include <cstdlib>
#include <new>
// One-shot allocation fault injection for the close-under-memory-pressure case.
static bool failNextAllocation = false;
void* operator new(std::size_t n) { if (failNextAllocation) { failNextAllocation = false; throw std::bad_alloc(); } if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
#include "../../src/tick_discovery.hpp"
#include <cstdio>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

using namespace TickDiscovery;
static int checks = 0;
static void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); ++checks; }
static const std::string Sha(64, 'a');
static const long long Now = 1791389326;
static std::string Text(std::string id = std::string(32, '1'), std::string seconds = "10", long long expires = Now + 300, std::string sha = Sha) {
    return "action=discover\nid=" + id + "\nseconds=" + seconds + "\nexpiresUnix=" + std::to_string(expires) + "\ngameSha256=" + sha + "\n";
}
static std::string Refusal(const std::string& text) { Request r; return ParseRequest(text, Now, Sha, r); }
static Observation Car(std::uintptr_t car = 0x1000) {
    Observation o; o.car = car; o.playerCar = true; o.carId = 3; o.carKind = 1; o.stage = 0; o.currentMode = 0x10;
    o.position = { 1.5f, 2.25f, -3.0f }; o.m70[0] = 1; o.mB0[5] = 1; o.mF0[15] = 1; o.speed = 12.5f;
    return o;
}
static std::string Read(const std::wstring& path) { std::ifstream f(path, std::ios::binary); std::stringstream s; s << f.rdbuf(); return s.str(); }
static bool Exists(const std::wstring& path) { return IsFile(path); }
static std::size_t Count(const std::string& s, char c) { std::size_t n = 0; for (char x : s) n += x == c; return n; }
static void Write(const std::wstring& path, const std::string& bytes) { std::ofstream f(path, std::ios::binary); f << bytes; }

static void Parsing() {
    Request r;
    Require(ParseRequest(Text(), Now, Sha, r).empty() && r.seconds == 10 && r.id == std::string(32, '1'), "valid request");
    std::string crlf = Text(); for (std::size_t i = crlf.find('\n'); i != std::string::npos; i = crlf.find('\n', i + 2)) crlf.insert(i, "\r");
    Require(Refusal(crlf).empty(), "CRLF request");
    Require(Refusal(std::string(MaxRequestBytes + 1, 'x')) == "request too large", "size bound");
    Require(Refusal("action\n") == "malformed line", "malformed line");
    Require(Refusal(Text() + "autoExit=true\n") == "unknown key", "unknown key refused");
    Require(Refusal(Text() + "seconds=20\n") == "repeated or empty key", "repeated key");
    Require(Refusal("action=\n") == "repeated or empty key", "empty value");
    Require(Refusal("action=observe\n") == "action must be discover", "action");
    Require(Refusal(Text(std::string(32, 'A'))) == "id must be 32 lowercase hex", "uppercase id");
    Require(Refusal(Text(std::string(31, '1'))) == "id must be 32 lowercase hex", "short id");
    Require(Refusal(Text(std::string(32, '1'), "9")) == "seconds out of range", "seconds below range");
    Require(Refusal(Text(std::string(32, '1'), "121")) == "seconds out of range", "seconds above range");
    Require(Refusal(Text(std::string(32, '1'), "060")) == "seconds out of range", "leading zero");
    Require(Refusal(Text(std::string(32, '1'), "-10")) == "seconds out of range", "negative seconds");
    Require(Refusal(Text(std::string(32, '1'), "10", Now)) == "request expired", "expiry now");
    Require(Refusal(Text(std::string(32, '1'), "10", Now + MaxLeadSeconds + 1)) == "expiry too far ahead", "expiry lead");
    Require(Refusal(Text(std::string(32, '1'), "10", Now + 60, std::string(64, 'b'))) == "game hash mismatch", "game hash");
}

static Session Started(int seconds = 10) { Session s; Request r{ std::string(32, '2'), seconds, Now + 60 }; Require(s.Begin(r, 0, false), "begin"); return s; }
static void CameraRows() {
    auto o = Car();
    Require(!CameraFinite(o.camera), "unobserved camera is not a measured zero");
    o.camera.observed = true; o.camera.mode = 2; o.camera.fov = 45;
    o.camera.znear = 0.25f; o.camera.zfar = 10000; o.camera.modeTimer = 3;
    o.camera.position = { 10, 20, 30 }; o.camera.look = { 40, 50, 60 }; o.camera.angle = { 70, 80, 90 };
    for (std::size_t m = 0; m < 7; ++m) for (std::size_t i = 0; i < 16; ++i) o.camera.matrices[m][i] = float(m * 100 + i + 1);
    Require(CameraFinite(o.camera), "all observed camera values finite");
    auto s = Started(); s.Observe(Phase::Pre, 0, o); s.Observe(Phase::Post, 0, o);
    std::istringstream stream(Tsv(s)); std::string header, row;
    std::getline(stream, header); std::getline(stream, row);
    auto split = [](const std::string& line) { std::vector<std::string> cells; std::istringstream in(line); std::string v; while (std::getline(in, v, '\t')) cells.push_back(v); return cells; };
    const auto names = split(header), values = split(row);
    Require(names.size() == 234 && values.size() == 234, "camera and force extension has fixed named shape");
    Require(names[72] == "camera_observed" && values[72] == "1" && values[73] == "1", "observed and finite flags serialized");
    Require(names[75] == "camera_fov_ac" && values[75] == "45" && values[76] == "0.25", "FOV and near plane preserved without unit conversion");
    Require(names[79] == "camera_pos_f8_x" && values[79] == "10" && names[87] == "camera_angle_128_z" && values[87] == "90", "position look angles keep boundaries");
    const char* offsets[] = { "140", "180", "1c0", "200", "240", "280", "2c0" };
    for (std::size_t m = 0; m < 7; ++m) for (std::size_t i = 0; i < 16; ++i) {
        const auto column = 88 + m * 16 + i;
        const auto expected = std::string("camera_") + offsets[m] + "_" + std::to_string(i / 4 + 1) + std::to_string(i % 4 + 1);
        Require(names[column] == expected && std::stof(values[column]) == o.camera.matrices[m][i], "camera matrix orientation and value round-trip");
        auto bad = o; bad.camera.matrices[m][i] = std::numeric_limits<float>::quiet_NaN();
        Require(!CameraFinite(bad.camera) && Finite(bad), "bad camera matrix does not discard qualified car fields");
    }
    auto bad = o; bad.camera.fov = std::numeric_limits<float>::infinity();
    auto partial = Started(); partial.Observe(Phase::Pre, 0, bad); partial.Observe(Phase::Post, 0, bad);
    Require(partial.failure.empty() && Tsv(partial).find("\t1\t0\t2\tinf\t") != std::string::npos, "invalid camera is explicitly marked and retained, not fatal to car discovery");
    Require(OutcomeText(s, "observed", "fixture", "fixture", "discovery.tsv").find("schema=outrun2006.tick-discovery@3\n") != std::string::npos,
        "new schema does not impersonate old car-only evidence");
}
static void ForceRows() {
    struct InputCar {
        unsigned field_8 = 0x12345678;
        float field_1C8 = 1, field_1CC = 2, field_1D0 = 3, field_1D4 = 4,
            field_1DC = 5, field_1E0 = 6, field_264 = 7, field_268 = 8;
        unsigned water_flag_24C[4] = {1,2,4,8};
        struct { int loadColiType_0 = 9; } OnRoadPlace_5C;
    } car;
    int calls = 0;
    auto lookup = [&](unsigned surface, int load, unsigned long* water) {
        Require(load == 9 && surface == unsigned(1 << calls), "lookup receives four ordered original masks and load type");
        ++calls; if (surface == 2) *water |= 1u;
        return surface == 2 ? 0.7 : surface == 4 ? 0.25 : 0.0;
    };
    OutRunForceObservation::Configuration config{};
    for (size_t i = 0; i < config.size(); ++i) config[i] = double(i) / 10;
    const auto sample = OutRunForceObservation::Read(&car, config, false, true, true, lookup);
    Require(calls == 4 && sample.observed && !sample.outputEnabled && sample.legacyRequested && sample.periodicRequested,
        "FFB off still captures inputs without an actuator or model initialization");
    Require(sample.flags == 0x12345678 && sample.config == config && sample.fields[2] == 3 && sample.fields[7] == 8,
        "raw fields and settings are read unchanged");
    Require(sample.roughness == 0.7f && sample.water == 1 && sample.surfaceMasks[2] == 4 && sample.loadColiType == 9,
        "shared max and accumulated local water flag retain surface meaning");
    Require(car.field_8 == 0x12345678 && car.field_1D0 == 3 && car.water_flag_24C[1] == 2,
        "observer leaves car fields unchanged");
    auto o = Car(); o.force = sample;
    auto s = Started(); s.Observe(Phase::Pre, 0, o); s.Observe(Phase::Post, 0, o);
    std::istringstream stream(Tsv(s)); std::string header, row;
    std::getline(stream, header); std::getline(stream, row);
    std::vector<std::string> names, values; std::string cell;
    std::istringstream hs(header), rs(row);
    while (std::getline(hs, cell, '\t')) names.push_back(cell);
    while (std::getline(rs, cell, '\t')) values.push_back(cell);
    Require(names.size() == 234 && values.size() == 234, "force row width matches schema");
    auto value = [&](const char* name) { auto found = std::find(names.begin(), names.end(), name); Require(found != names.end(), "named force column exists"); return std::stod(values[size_t(found - names.begin())]); };
    Require(value("force_inputs_observed") == 1 && value("force_inputs_finite") == 1 && value("force_output_enabled") == 0,
        "actual off flag is not rewritten as muted enabled output");
    Require(value("field_8") == 0x12345678 && value("field_1d4") == 4 && value("surface_water") == 1,
        "force integer and scalar inputs round-trip");
    for (size_t i = 0; i < config.size(); ++i)
        Require(value(OutRunForceObservation::ConfigNames[i]) == config[i], "configuration doubles round-trip without rounding");
    for (size_t i = 0; i < sample.fields.size(); ++i) {
        auto bad = sample; bad.fields[i] = std::numeric_limits<float>::quiet_NaN();
        Require(!OutRunForceObservation::Finite(bad), "non-finite raw force field is explicitly unavailable");
    }
    for (size_t i = 0; i < config.size(); ++i) {
        auto bad = sample; bad.config[i] = std::numeric_limits<double>::infinity();
        Require(!OutRunForceObservation::Finite(bad), "non-finite tuning is explicitly unavailable");
    }
    o.force.fields[0] = std::numeric_limits<float>::quiet_NaN();
    auto partial = Started(); partial.Observe(Phase::Pre, 0, o); partial.Observe(Phase::Post, 0, o);
    Require(partial.failure.empty() && Finite(o) && Tsv(partial).find("\t1\t0\t0\t1\t1\t305419896\t") != std::string::npos,
        "invalid force values are retained with a false finite flag without losing pose discovery");
    Require(OutcomeText(s, "observed", "fixture", "fixture", "discovery.tsv").find("no calculation, native route or delivered command inferred") != std::string::npos,
        "outcome does not claim original force or route");
}
static void Ordering() {
    { Session s = Started(); s.Update(1, true); s.Observe(Phase::Pre, 1, Car()); s.Observe(Phase::Post, 1, Car());
      s.Update(2, true); s.Update(3, false); s.Close();
      Require(s.failure.empty() && s.rows.size() == 2 && s.totalPairs == 1 && s.gameUpdates == 2 && s.gameUpdatesWithoutPair == 1 &&
              s.updates == 4 && s.maxPairs == 1 && !s.UnmatchedPre(), "pairs and per-update accounting (arming update included)"); }
    { Session s = Started(); s.Update(1, true); s.Observe(Phase::Pre, 1, Car()); s.Observe(Phase::Pre, 1, Car());
      Require(s.failure == "two pre observations" && s.rows.size() == 1, "two pre"); }
    { Session s = Started(); s.Update(1, true); s.Observe(Phase::Post, 1, Car()); Require(s.failure == "post without pre" && s.rows.empty(), "post without pre"); }
    { Session s = Started(); s.Update(1, true); s.Observe(Phase::Pre, 1, Car()); s.Observe(Phase::Post, 1, Car(0x2000));
      Require(s.failure == "post on a different car", "post car identity"); }
    { Session s = Started(); s.Update(1, true); auto o = Car(); o.playerCar = false; s.Observe(Phase::Pre, 1, o);
      Require(s.failure == "car is not the local player (event 8)", "other car refused"); }
    { Session s = Started(); s.Update(1, true); auto o = Car(); o.network = true; s.Observe(Phase::Pre, 1, o);
      Require(s.failure == "network driver or lobby active", "network refused"); }
    { Session s = Started(); s.Update(1, true); auto o = Car(); o.mF0[3] = std::numeric_limits<float>::quiet_NaN(); s.Observe(Phase::Pre, 1, o);
      Require(s.failure == "non-finite value" && s.rows.empty(), "non-finite refused"); }
    { Session s = Started(); s.Update(1, true); s.Observe(Phase::Pre, 1, Car()); s.Update(2, true);
      Require(s.failure == "pre without post across updates" && s.UnmatchedPre(), "pre across updates"); }
    { Session s = Started(); s.Update(2, true); Require(s.failure == "update counter skipped or reordered", "skipped update"); }
    { Session s = Started(); s.Update(1, true); s.Observe(Phase::Pre, 2, Car()); Require(s.failure == "observation outside the current update", "stale update"); }
    { Session s = Started(); s.Observe(Phase::Pre, 0, Car()); s.Observe(Phase::Post, 0, Car());
      Require(s.failure.empty() && s.rows.size() == 2, "the arming update's own car tick is recorded"); }
    { Session s = Started(); s.Update(1, true); s.Observe(Phase::Pre, 1, Car()); auto o = Car(); o.stage = 1; s.Observe(Phase::Post, 1, o);
      s.Observe(Phase::Pre, 1, Car());
      Require(s.ended == "stage changed" && s.failure.empty() && s.rows.size() == 2 && s.rows[1].o.stage == 1, "stage change ends after its row"); }
    { Session s = Started(); s.Update(1, true); s.Observe(Phase::Pre, 1, Car()); auto o = Car(); o.carColour = 4; s.Observe(Phase::Post, 1, o);
      Require(s.ended == "car changed", "car change ends"); }
    { Session s = Started(); std::uint32_t u = 0;
      while (s.rows.size() < s.capacity) { s.Update(++u, true); s.Observe(Phase::Pre, u, Car()); s.Observe(Phase::Post, u, Car()); }
      Require(s.failure.empty() && s.capacity == 10u * UpdatesPerSecond * 2 + Headroom, "capacity reached without failure");
      s.Update(++u, true); s.Observe(Phase::Pre, u, Car()); Require(s.failure == "capacity" && s.rows.size() == s.capacity, "capacity never wraps"); }
    // Same-model replacement of the local car object (finding 4): the changing row is kept, then the window ends.
    { Session s = Started(); s.Observe(Phase::Pre, 0, Car(0x1000)); s.Observe(Phase::Post, 0, Car(0x1000));
      s.Update(1, true); s.Observe(Phase::Pre, 1, Car(0x2000));
      Require(s.ended == "car instance changed" && s.failure.empty() && s.rows.size() == 3 && s.rows.back().instance == 1 && s.rows.front().instance == 0,
              "same-model car replacement ends with its row and instance epoch");
      const auto text = Tsv(s); Require(text.find("\npre\t1\t1\t") != std::string::npos, "car instance serialized"); }
    { Session s = Started(); for (std::uint32_t u = 0; u < 5; ++u) { if (u) s.Update(u, true); s.Observe(Phase::Pre, u, Car(0x1000)); s.Observe(Phase::Post, u, Car(0x1000)); }
      Require(!s.Stopped() && s.rows.size() == 10, "same car instance across updates continues"); }    { Session s = Started(); for (std::uint32_t u = 1; u < 600; ++u) s.Update(u, false);
      Require(!s.Due(599) && s.Due(600) && s.failure.empty(), "window is updates 0..599 for 10 s"); }
}

static void Files(const std::wstring& base) {
    const std::wstring root = base + L"\\outrun-discovery";
    Require(CreateDirectoryW(root.c_str(), nullptr) != FALSE, "fixture root");
    Controller c; c.root = root; c.hooks = "directInputFfbCarHook:1 vibrationCarHookEnabled:0 chainOrder:unobserved";
    bool idle = true; for (int i = 0; i < 120; ++i) idle = c.OnUpdate(false, false, Now, Sha, "").empty() && idle;
    Require(idle, "idle without a request");

    const std::string idA(32, 'a');
    Write(root + L"\\request.txt", Text(idA));
    bool waiting = true; for (int i = 0; i < 59; ++i) waiting = c.OnUpdate(false, false, Now, Sha, "").empty() && !c.session && waiting;
    Require(waiting, "request read only once per 60 updates");
    Require(c.OnUpdate(false, false, Now, Sha, "").find("armed") != std::string::npos && c.session, "valid request arms");
    const std::wstring dirA = root + L"\\" + std::wstring(idA.begin(), idA.end());
    Require(!Exists(root + L"\\request.txt") && Read(dirA + L"\\request.txt") == Text(idA), "request claimed into its result folder");
    std::uint32_t armedAt = c.update;
    for (int u = 0; u < 3; ++u) { c.OnUpdate(true, false, Now, Sha, ""); c.Observe(Phase::Pre, Car()); c.Observe(Phase::Post, Car()); }
    while (c.session) c.OnUpdate(false, false, Now, Sha, "");
    Require(c.update - armedAt == 600, "window closes after exactly seconds x 60 updates");
    const std::string outcome = Read(dirA + L"\\outcome.txt"), tsv = Read(dirA + L"\\discovery.tsv");
    Require(outcome.find("outcome=observed\n") != std::string::npos && outcome.find("detail=duration ended\n") != std::string::npos &&
            outcome.find("rows=6\n") != std::string::npos && outcome.find("pairs=3\n") != std::string::npos &&
            outcome.find("gameUpdates=3\n") != std::string::npos && outcome.find("updates=600\n") != std::string::npos && outcome.find("gameUpdatesWithoutPair=0\n") != std::string::npos &&
            outcome.find("replayable=false\n") != std::string::npos && outcome.find("chainOrder:unobserved") != std::string::npos, "observed outcome");
    Require(Count(tsv, '\n') == 7, "header plus six rows");
    const std::string header = tsv.substr(0, tsv.find('\n')), first = tsv.substr(header.size() + 1, tsv.find('\n', header.size() + 1) - header.size() - 1);
    Require(Count(header, '\t') == 233 && Count(first, '\t') == 233 && header.rfind("phase\tupdate\tcar_instance\t", 0) == 0, "234 named columns in every row, original 200 car/camera columns retained");
    Require(first.rfind("pre\t", 0) == 0 && first.find("\t1.5\t2.25\t-3\t") != std::string::npos, "values round-trip as written");
    Require(!Exists(dirA + L"\\discovery.tsv.tmp") && !Exists(dirA + L"\\outcome.txt.tmp"), "no temporary files left");

    // Same id again: its result folder exists, so the request is refused and nothing in it changes.
    Write(root + L"\\request.txt", Text(idA));
    while (c.update % UpdatesPerSecond != UpdatesPerSecond - 1) c.OnUpdate(false, false, Now, Sha, "");
    Require(c.OnUpdate(false, false, Now, Sha, "").find("result folder exists") != std::string::npos && !c.session, "existing result refused");
    Require(Read(dirA + L"\\outcome.txt") == outcome && Exists(root + L"\\request.refused.txt") && !Exists(root + L"\\request.txt") &&
            Read(root + L"\\refused.txt") == "refused=result folder exists or cannot be created\n", "refusal leaves the earlier result unchanged");

    auto armNext = [&](const std::string& id, const char* unavailable = "") {
        Write(root + L"\\request.txt", Text(id));
        while (c.update % UpdatesPerSecond != UpdatesPerSecond - 1) c.OnUpdate(false, false, Now, Sha, "");
        return c.OnUpdate(false, false, Now, Sha, unavailable);
    };
    Require(armNext(std::string(32, 'b'), "car hook inactive").find("car hook inactive") != std::string::npos && !c.session, "unavailable build refuses");
    Require(armNext(std::string(32, 'c')).find("armed") != std::string::npos, "arm for stop");
    const std::wstring dirC = root + L"\\" + std::wstring(32, L'c');
    Write(dirC + L"\\stop.txt", "stop\n");
    while (c.session) c.OnUpdate(false, false, Now, Sha, "");
    Require(Read(dirC + L"\\outcome.txt").find("outcome=stopped\n") != std::string::npos, "external stop");

    Require(armNext(std::string(32, '7')).find("armed") != std::string::npos, "arm, then observe in the arming update");
    c.Observe(Phase::Pre, Car()); c.Observe(Phase::Post, Car());
    Require(c.session && c.session->failure.empty() && c.session->rows.size() == 2, "arming update observed without failure");
    Write(root + L"\\" + std::wstring(32, L'7') + L"\\stop.txt", "stop\n");
    while (c.session) c.OnUpdate(false, false, Now, Sha, "");
    { const std::string o7 = Read(root + L"\\" + std::wstring(32, L'7') + L"\\outcome.txt");
      Require(o7.find("outcome=stopped\n") != std::string::npos && o7.find("rows=2\n") != std::string::npos && o7.find("gameUpdatesWithoutPair=0\n") != std::string::npos, "stopped window keeps its rows"); }
    Require(armNext(std::string(32, 'd')).find("armed") != std::string::npos, "arm for network");
    c.OnUpdate(true, true, Now, Sha, "");
    Require(!c.session && Read(root + L"\\" + std::wstring(32, L'd') + L"\\outcome.txt").find("detail=network driver or lobby active\n") != std::string::npos, "network at update fails");

    Require(armNext(std::string(32, 'e')).find("armed") != std::string::npos, "arm for observe failure");
    c.OnUpdate(true, false, Now, Sha, ""); c.Observe(Phase::Post, Car());
    Require(c.Finalize().find("failed (post without pre)") != std::string::npos, "exit keeps a recorded failure");
    Require(Read(root + L"\\" + std::wstring(32, L'e') + L"\\outcome.txt").find("outcome=failed\n") != std::string::npos, "failure written at exit");
    Require(c.Finalize().empty() && c.finalized, "finalize once");
    Write(root + L"\\request.txt", Text(std::string(32, 'f')));
    for (int i = 0; i < 120; ++i) c.OnUpdate(false, false, Now, Sha, "");
    Require(!c.session && Exists(root + L"\\request.txt"), "nothing arms after finalization");

    // Closing under failure (Astra's 2026-10-07 review findings 1-3, inverted to required outcomes).
    auto armAt = [&](Controller& k, const std::wstring& r, char id) {
        Require(CreateDirectoryW(r.c_str(), nullptr) != FALSE, "isolated root"); k.root = r;
        Write(r + L"\\request.txt", Text(std::string(32, id)));
        for (int i = 0; i < 60; ++i) k.OnUpdate(true, false, Now, Sha, "");
        Require(k.session != nullptr, "armed");
    };
    auto fill = [&](Controller& k, bool lastPreOnly) {
        for (int i = 0; i < 600; ++i) { if (i) k.OnUpdate(true, false, Now, Sha, ""); k.Observe(Phase::Pre, Car()); if (!lastPreOnly || i != 599) k.Observe(Phase::Post, Car()); }
    };
    { Controller k; armAt(k, base + L"\\memory", 'a'); k.Observe(Phase::Pre, Car()); k.Observe(Phase::Post, Car()); const auto d = k.dir;
      bool threw = false; failNextAllocation = true;
      try { k.Finalize(); } catch (...) { threw = true; }
      failNextAllocation = false;
      const auto o = Read(d + L"\\outcome.txt");
      Require(!threw && !k.session && k.finalized && o.find("outcome=failed\n") != std::string::npos && o.find("could not be serialized (memory)") != std::string::npos,
              "an allocation failure while closing is contained and recorded without allocating"); }
    { Controller k; armAt(k, base + L"\\retry", 'b'); const auto d = k.dir;
      Require(CreateDirectoryW((d + L"\\discovery.tsv").c_str(), nullptr) != FALSE, "block the data file");
      fill(k, false); const auto log = k.OnUpdate(true, false, Now, Sha, ""); const auto o = Read(d + L"\\outcome.txt");
      Require(o.find("outcome=observed\n") != std::string::npos && o.find("dataFile=discovery.retry.tsv\n") != std::string::npos &&
              IsFile(d + L"\\discovery.retry.tsv") && log.find("discovery.retry.tsv") != std::string::npos, "a blocked data file is retried once under a new name"); }
    { Controller k; armAt(k, base + L"\\nodata", 'c'); const auto d = k.dir;
      CreateDirectoryW((d + L"\\discovery.tsv").c_str(), nullptr); CreateDirectoryW((d + L"\\discovery.retry.tsv").c_str(), nullptr);
      fill(k, false); k.OnUpdate(true, false, Now, Sha, ""); const auto o = Read(d + L"\\outcome.txt");
      Require(o.find("outcome=failed\n") != std::string::npos && o.find("data file could not be written (window observed: duration ended)") != std::string::npos &&
              o.find("dataFile=none\n") != std::string::npos && !k.session, "no committed data: failed, never observed"); }
    { Controller k; armAt(k, base + L"\\nooutcome", 'd'); const auto d = k.dir;
      CreateDirectoryW((d + L"\\outcome.txt").c_str(), nullptr);
      fill(k, false); const auto log = k.OnUpdate(true, false, Now, Sha, "");
      Require(IsFile(d + L"\\discovery.tsv") && log.find("OUTCOME WRITE FAILED") != std::string::npos && !k.session, "an outcome write failure is reported, the data kept"); }
    { Controller k; armAt(k, base + L"\\unmatched", 'e'); const auto d = k.dir; fill(k, true); k.OnUpdate(true, false, Now, Sha, "");
      const auto o = Read(d + L"\\outcome.txt");
      Require(o.find("outcome=failed\n") != std::string::npos && o.find("pre without post at the window end") != std::string::npos, "an unmatched final pre fails the window"); }
    { Controller k; armAt(k, base + L"\\stoppending", 'f'); const auto d = k.dir;   // armed at update 60
      k.Observe(Phase::Pre, Car()); k.Observe(Phase::Post, Car());
      for (int u = 61; u < 119; ++u) { k.OnUpdate(true, false, Now, Sha, ""); k.Observe(Phase::Pre, Car()); k.Observe(Phase::Post, Car()); }
      k.OnUpdate(true, false, Now, Sha, ""); k.Observe(Phase::Pre, Car());          // update 119: a pre, then the stop check at 120
      Write(d + L"\\stop.txt", "stop\n"); k.OnUpdate(true, false, Now, Sha, "");
      const auto o = Read(d + L"\\outcome.txt");
      Require(!k.session && o.find("outcome=stopped\n") != std::string::npos && o.find("unmatchedPre=true\n") != std::string::npos,
              "a stop with an open pre stays stopped and says the pre is unmatched"); }
    Controller leaving; leaving.root = root; DeleteFileW((root + L"\\request.txt").c_str());
    Write(root + L"\\request.txt", Text(std::string(32, '9')));
    for (int i = 0; i < 60; ++i) leaving.OnUpdate(false, false, Now, Sha, "");
    leaving.OnUpdate(true, false, Now, Sha, ""); leaving.Observe(Phase::Pre, Car());
    Require(leaving.Finalize().find("exit (game exited before the window ended), 1 rows") != std::string::npos, "normal exit writes incomplete");
    const std::string exitOutcome = Read(root + L"\\" + std::wstring(32, L'9') + L"\\outcome.txt");
    Require(exitOutcome.find("outcome=exit\n") != std::string::npos && exitOutcome.find("unmatchedPre=true\n") != std::string::npos, "exit outcome labels the open pre");
}

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("usage: tick-discovery <empty temp folder>");
        std::string a(argv[1]);
        Parsing(); Ordering(); CameraRows(); ForceRows(); Files(std::wstring(a.begin(), a.end()));
        std::printf("PASS %d tick discovery checks (memory and temp files only; no game, hook or device)\n", checks);
        return 0;
    } catch (const std::exception& e) { std::fprintf(stderr, "FAIL after %d checks: %s\n", checks, e.what()); return 1; }
}
