#pragma once
#include <condition_variable>
#include <mutex>
#include <atomic>
#include <cstddef>
#include <thread>

// Consumer-owned admission/drain barrier. No toolkit worker or game resource
// ownership is implied. None of these operations may be called from DllMain.
namespace ConsumerLifecycle {
class Gate {
public:
    enum class Phase { Running, Paused, Stopping, Stopped };
private:
    std::mutex state_;
    std::condition_variable drained_;
    std::recursive_mutex operations_;
    Phase phase_ = Phase::Running;
    std::size_t active_ = 0;
    bool finalizing_ = false;
    std::thread::id finalizer_;
    bool deferredSilence_ = false, deferredResume_ = false;
    void (*idle_)() = nullptr;
    inline static thread_local std::size_t depth_ = 0;
public:
    class Lease {
        Gate& gate_;
        std::unique_lock<std::recursive_mutex> operation_;
        bool admitted_ = false;
    public:
        explicit Lease(Gate& gate, bool control = false) : gate_(gate), operation_(gate.operations_, std::defer_lock) {
            {
                std::lock_guard lock(gate_.state_);
                if (gate_.phase_ != Phase::Running && !(control && gate_.phase_ == Phase::Paused)) return;
                ++gate_.active_; // Includes queued operations, so shutdown drains them too.
            }
            operation_.lock();
            {
                std::lock_guard lock(gate_.state_);
                admitted_ = gate_.phase_ == Phase::Running || (control && gate_.phase_ == Phase::Paused);
                if (admitted_) ++depth_;
            }
            if (!admitted_) { operation_.unlock(); gate_.Release(false); }
        }
        ~Lease() { if (admitted_) { --depth_; operation_.unlock(); gate_.Release(true); } }
        explicit operator bool() const { return admitted_; }
        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;
    };
    void Release(bool) {
        void (*idle)() = nullptr;
        {
            std::lock_guard lock(state_);
            --active_;
            if (!active_) {
                drained_.notify_all();
                if (deferredSilence_ && !depth_) {
                    deferredSilence_ = false; idle = idle_;
                    if (idle) ++active_; // Reserve reconciliation before unlocking admission.
                }
            }
        }
        // No admission/producer lock held across the deferred native callback.
        if (idle) { idle(); Release(false); return; }
        {
            std::lock_guard lock(state_);
            if (!active_ && deferredResume_ && phase_ == Phase::Paused) {
                deferredResume_ = false; phase_ = Phase::Running;
            }
        }
    }
    void SetIdleCallback(void (*callback)()) { std::lock_guard lock(state_); idle_ = callback; }
    void DeferUntilIdle() { std::lock_guard lock(state_); deferredSilence_ = true; }
    bool Pause() {
        std::lock_guard lock(state_);
        if (phase_ == Phase::Stopping || phase_ == Phase::Stopped) return false;
        phase_ = Phase::Paused;
        deferredResume_ = false; // A fresh request supersedes an earlier cancellation.
        // A window callback must not block on a producer that may itself be
        // waiting for the owner thread to dispatch a message.
        if (active_) { deferredSilence_ = true; return false; }
        return true;
    }
    void Resume() {
        void (*idle)() = nullptr;
        {
            std::lock_guard lock(state_);
            if (phase_ != Phase::Paused) return;
            deferredResume_ = true;
            if (active_) { deferredSilence_ = true; return; }
            deferredSilence_ = false;
            idle = idle_;
            if (idle) ++active_; // A concurrent cancellation cannot bypass its callback.
        }
        // Stay Paused while control work reconciles a deferred selection. This
        // is also required when cancellation arrives after the producer ended.
        if (idle) { idle(); Release(false); return; }
        {
            std::lock_guard lock(state_);
            if (!active_ && deferredResume_ && phase_ == Phase::Paused) {
                deferredResume_ = false; phase_ = Phase::Running;
            }
        }
    }
    bool ResumeRequested() { std::lock_guard lock(state_); return deferredResume_ && phase_ == Phase::Paused; }
    void RequestStop() { std::lock_guard lock(state_); if (phase_ != Phase::Stopped) phase_ = Phase::Stopping; }
    bool ClaimFinalization() {
        if (depth_) { RequestStop(); return false; } // Never wait on this thread's lease.
        std::unique_lock lock(state_);
        if (phase_ == Phase::Stopped) return false;
        phase_ = Phase::Stopping;
        if (finalizing_) {
            if (finalizer_ == std::this_thread::get_id()) return false;
            drained_.wait(lock, [&] { return phase_ == Phase::Stopped; }); return false;
        }
        finalizing_ = true;
        finalizer_ = std::this_thread::get_id();
        drained_.wait(lock, [&] { return active_ == 0; });
        return true;
    }
    void CompleteFinalization() { std::lock_guard lock(state_); phase_ = Phase::Stopped; drained_.notify_all(); }
    Phase Current() { std::lock_guard lock(state_); return phase_; }
    static bool Reentrant() { return depth_ != 0; }
};
// Deliberately retained until process termination: no mutex/condition-variable
// destructor participates in DLL_PROCESS_DETACH.
inline Gate& Runtime() { static auto* const gate = new Gate; return *gate; }
inline std::atomic<bool> hostVerified = false;
inline bool (*actuatorReadiness)(void*) = nullptr;
inline bool ReadyForActuator(void* hwnd) { return hostVerified.load() && actuatorReadiness && actuatorReadiness(hwnd); }
}
