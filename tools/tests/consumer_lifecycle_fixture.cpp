// Production gate and FFB entry points, fake ABI only. No native DLL/device/game.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
static HWND FixtureForeground() { return reinterpret_cast<HWND>(1); }
#define GetForegroundWindow FixtureForeground
#include "../../src/hooks_dinputffb.cpp"
namespace TickDiscovery { void Observe(EVWORK_CAR*, bool) {} void NoteHooks(bool, bool) {} } // inert without an armed window
#include <cassert>
#include <future>
#include <iostream>
#include <vector>
#include <chrono>
using namespace std::chrono_literals;
float VibrationLeftMotor = 0, VibrationRightMotor = 0;
double __cdecl sub_1149C0(unsigned int, int, DWORD*) { return 0; }
Hook::Hook() {}
namespace DInputRemap {
IDirectInputDevice8A* GetPrimaryDevice() { return nullptr; }
bool IsPrimaryInitialized() { return false; }
bool GetPrimaryDeviceGuid(GUID*) { return false; }
int GetTelemetryAccel() { return -1; }
int GetTelemetryBrake() { return -1; }
UiSnapshot ReadUiSnapshot() { return {}; }
}
static std::vector<int> events;
static std::promise<void> entered, releaseOutput;
static std::shared_future<void> releaseSignal;
static bool delayed = false, reentrantClose = false, reentrantSwitch = false;
static int __cdecl Output(int force, int) {
    events.push_back(force ? 1 : 2);
    if (delayed) { entered.set_value(); assert(releaseSignal.wait_for(3s) == std::future_status::ready); events.push_back(3); }
    if (reentrantClose) {
        reentrantClose = false;
        assert(!ConsumerLifecycle::Runtime().Pause());
        assert(!ConsumerLifecycle::Runtime().ClaimFinalization()); // never waits on own lease
    }
    if (reentrantSwitch) { reentrantSwitch = false; FFB::SelectionChanged(); assert(FFB::initialized); }
    return 1;
}
static void __cdecl Zero() { events.push_back(4); }
static void __cdecl Free() { events.push_back(5); }
static void __cdecl Panic() { events.push_back(6); assert(!ConsumerLifecycle::Runtime().ClaimFinalization()); }
static ConsumerLifecycle::Gate* reconciliationGate = nullptr;
static std::promise<void> beforeReconciliation, allowReconciliation;
static std::shared_future<void> reconcileSignal;
static std::atomic<int> reconciliationCalls = 0;
static void ReconciliationBarrier() {
    if (++reconciliationCalls == 1) {
        beforeReconciliation.set_value();
        assert(reconcileSignal.wait_for(3s)==std::future_status::ready);
    }
    ConsumerLifecycle::Gate::Lease control(*reconciliationGate, true);
    assert(control && reconciliationGate->Current()==ConsumerLifecycle::Gate::Phase::Paused);
}

int main() {
    using namespace ConsumerLifecycle;
    // Isolated gate: canceled request and nested silence defer until outer lease.
    Gate local;
    { Gate::Lease first(local); assert(first); assert(!local.Pause()); local.Resume();
      Gate::Lease blocked(local); assert(!blocked); }
    assert(local.Current() == Gate::Phase::Running);
    assert(local.Pause()); local.Resume(); assert(local.Current() == Gate::Phase::Running);
    // Delayed producer/queued work drain without holding the operation mutex.
    Gate concurrent; std::promise<void> inside, finish;
    auto finishSignal = finish.get_future().share();
    auto producer = std::async(std::launch::async, [&] { Gate::Lease lease(concurrent); assert(lease); inside.set_value(); assert(finishSignal.wait_for(3s)==std::future_status::ready); });
    assert(inside.get_future().wait_for(3s)==std::future_status::ready);
    auto queued = std::async(std::launch::async, [&] { Gate::Lease lease(concurrent); assert(!lease); });
    assert(queued.wait_for(30ms)==std::future_status::timeout);
    auto finalizer = std::async(std::launch::async, [&] { assert(concurrent.ClaimFinalization()); concurrent.CompleteFinalization(); });
    const auto deadline=std::chrono::steady_clock::now()+3s;
    while(concurrent.Current()!=Gate::Phase::Stopping && std::chrono::steady_clock::now()<deadline) std::this_thread::yield();
    assert(concurrent.Current()==Gate::Phase::Stopping);
    auto otherFinalizer=std::async(std::launch::async, [&] { assert(!concurrent.ClaimFinalization()); });
    { Gate::Lease refused(concurrent); assert(!refused); }
    assert(finalizer.wait_for(30ms)==std::future_status::timeout);
    finish.set_value(); assert(producer.wait_for(3s)==std::future_status::ready); producer.get();
    assert(finalizer.wait_for(3s)==std::future_status::ready); finalizer.get();
    assert(queued.wait_for(3s)==std::future_status::ready); queued.get();
    assert(otherFinalizer.wait_for(3s)==std::future_status::ready); otherFinalizer.get();
    // Cancellation racing an idle callback before that callback takes its own
    // control lease must not reopen admission around unfinished reconciliation.
    Gate reconciling; reconciliationGate=&reconciling;
    reconciling.SetIdleCallback(ReconciliationBarrier);
    reconcileSignal=allowReconciliation.get_future().share();
    auto reconcileProducer=std::async(std::launch::async,[&] {
        Gate::Lease outer(reconciling); assert(!reconciling.Pause());
    });
    assert(beforeReconciliation.get_future().wait_for(3s)==std::future_status::ready);
    reconciling.Resume(); assert(reconciling.Current()==Gate::Phase::Paused);
    { Gate::Lease denied(reconciling); assert(!denied); }
    allowReconciliation.set_value();
    assert(reconcileProducer.wait_for(3s)==std::future_status::ready); reconcileProducer.get();
    assert(reconciling.Current()==Gate::Phase::Running && reconciliationCalls==2);

    HWND hwnd = reinterpret_cast<HWND>(1); Game::hWnd_ptr = &hwnd;
    Settings::TelemetryEnabled = false;
    Runtime().SetIdleCallback(FFB::LifecycleIdle);
    FFB::ffbLoaded = true; FFB::initialized = true;
    FFB::ffb.SetDeviceForcesXY = Output; FFB::ffb.ZeroForces = Zero;
    FFB::ffb.FreeDirectInput = Free; FFB::ffb.PanicStop = Panic;
    // Unknown host and startup failure cannot emit output, even with a fake API.
    hostVerified = false; FFB::SetConstantForce(900); assert(events.empty());
    hostVerified = true; actuatorReadiness = [](void*) { return false; };
    FFB::SetConstantForce(900); assert(events.empty());
    actuatorReadiness = [](void*) { return true; };
    Settings::FFBGlobalStrength = 1;
    // Device switch waits for old output, zeroes first, releases and stays resumable.
    delayed = true; releaseSignal = releaseOutput.get_future().share();
    auto output = std::async(std::launch::async, [] { FFB::SetConstantForce(900); });
    assert(entered.get_future().wait_for(3s)==std::future_status::ready);
    auto change = std::async(std::launch::async, [] { FFB::SelectionChanged(); });
    assert(change.wait_for(30ms)==std::future_status::timeout);
    delayed = false; releaseOutput.set_value();
    assert(output.wait_for(3s)==std::future_status::ready); output.get();
    assert(change.wait_for(3s)==std::future_status::ready); change.get();
    assert((events == std::vector<int>{1,3,2,5}));
    assert(Runtime().Current()==Gate::Phase::Running && !FFB::initialized && !FFB::initAttempted);
    // Reentrant selection is processed only after the fake native call returns.
    events.clear(); FFB::initialized = true; reentrantSwitch = true;
    FFB::SetConstantForce(900);
    assert((events == std::vector<int>{1,2,5})); assert(!FFB::initialized);
    // Reentrant close is reversible, with deferred silence and no resource free.
    events.clear(); FFB::initialized = true;
    { Gate::Lease lease(Runtime()); assert(!Runtime().Pause()); Runtime().Resume(); }
    assert((events == std::vector<int>{4})); assert(Runtime().Current()==Gate::Phase::Running);
    // Review regression: pending selection overlapping a canceled close/session.
    // Exercise cancellation both inside the original producer and after it ends.
    for (const bool lateCancellation : {false, true}) {
        events.clear(); FFB::initialized = FFB::initAttempted = true;
        FFB::periodicsActive = true; FFB::slotRoadTexture = 0; FFB::slotTireSlip = 1;
        FFB::prevConstantLevel = FFB::prevStructLevel = 900;
        {
            Gate::Lease outer(Runtime());
            FFB::SelectionChanged(); assert(FFB::selectionPending);
            assert(!Runtime().Pause());
            if (!lateCancellation) Runtime().Resume();
        }
        if (lateCancellation) {
            assert(Runtime().Current()==Gate::Phase::Paused);
            assert(FFB::selectionPending && FFB::initialized && FFB::initAttempted);
            assert((events==std::vector<int>{4}));
            FFB::SetConstantForce(900); assert(events.size()==1);
            Runtime().Resume();
            assert((events==std::vector<int>{4,4,5}));
        } else assert((events==std::vector<int>{4,5}));
        assert(Runtime().Current()==Gate::Phase::Running);
        assert(!FFB::selectionPending && !FFB::initialized && !FFB::initAttempted);
        assert(!FFB::periodicsActive && FFB::slotRoadTexture==-1 && FFB::slotTireSlip==-1);
        assert(FFB::prevConstantLevel==0 && FFB::prevStructLevel==0);
        const auto reconciled=events.size();
        FFB::SetConstantForce(900); FFB::UpdatePeriodic(0,1,25);
        assert(events.size()==reconciled); // A freed selection cannot admit output.
    }
    // Terminal request from inside native callback cannot self-wait. Outer
    // boundary owns the eventual finalization; every producer then refuses work.
    events.clear(); FFB::initialized = true; reentrantClose = true; FFB::SetConstantForce(900);
    assert(Runtime().Current()==Gate::Phase::Stopping);
    assert(Runtime().ClaimFinalization()); FFB::FinalizeForExit(); Runtime().CompleteFinalization();
    assert((events == std::vector<int>{1,6,5}));
    const auto count = events.size(); EVWORK_CAR car{};
    FFB::Update(&car); FFB::CheckWatchdog(); FFB::RefreshUiDevices();
    FFB::SelectionChanged(); FFB::ZeroAllForces(); FFB::SilenceForLifecycle();
    Telemetry::SetEnabled(true); assert(!Settings::TelemetryEnabled);
    assert(!FFB::DeferredInit() && !Runtime().ClaimFinalization());
    assert(events.size()==count);
    std::cout << "PASS: production gate drain/reentrancy; unknown-host output refusal; delayed/reentrant device switch; selection plus canceled close/session reconciles before admission (deferred and later cancellation); freed-device constant/periodic refusal; reversible silence; once-only finalization; stopped producer rejection. Fake ABI only.\n";
}
