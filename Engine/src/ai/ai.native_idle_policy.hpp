// Epoch Engine - SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <utility>

namespace epochengine::ai::native_runtime::idle {

using Clock = std::chrono::steady_clock;

// A retirement claim is a short admission boundary, not a scheduler or a
// model manager. No caller holds this mutex while joining a worker.
class WorkGate final {
public:
    [[nodiscard]] bool acquire() {
        std::scoped_lock lock(mutex_);
        ++users_;
        return !retiring_;
    }
    void release() {
        std::scoped_lock lock(mutex_);
        --users_;
    }
    [[nodiscard]] bool protected_work() const {
        std::scoped_lock lock(mutex_);
        return users_ != 0u;
    }
    [[nodiscard]] bool try_claim_retirement() {
        std::scoped_lock lock(mutex_);
        if (users_ != 0u || retiring_) return false;
        retiring_ = true;
        return true;
    }
    void finish_retirement() {
        std::scoped_lock lock(mutex_);
        retiring_ = false;
    }
private:
    mutable std::mutex mutex_;
    std::uint64_t users_{};
    bool retiring_{};
};

inline WorkGate work_gate;

class WorkLease final {
public:
    explicit WorkLease(WorkGate& gate = work_gate)
        : gate_(&gate), admitted_(gate.acquire()) {}
    ~WorkLease() { if (gate_) gate_->release(); }
    WorkLease(WorkLease&& other) noexcept
        : gate_(std::exchange(other.gate_, nullptr)), admitted_(other.admitted_) {}
    WorkLease(const WorkLease&) = delete;
    WorkLease& operator=(const WorkLease&) = delete;
    WorkLease& operator=(WorkLease&&) = delete;
    // Work arriving after retirement was claimed has no loaded-model admission.
    // It must remain queued or fail before transport; it never triggers a load.
    [[nodiscard]] bool admitted() const noexcept { return admitted_; }
private:
    WorkGate* gate_{};
    bool admitted_{};
};

class RetirementClaim final {
public:
    explicit RetirementClaim(WorkGate& gate = work_gate)
        : gate_(gate.try_claim_retirement() ? &gate : nullptr) {}
    ~RetirementClaim() { if (gate_) gate_->finish_retirement(); }
    RetirementClaim(const RetirementClaim&) = delete;
    RetirementClaim& operator=(const RetirementClaim&) = delete;
    [[nodiscard]] explicit operator bool() const noexcept { return gate_ != nullptr; }
private:
    WorkGate* gate_{};
};

struct UsefulActivity final {
    Clock::time_point last_use{};
    bool observed{};
    bool retirement_failed{};
    void note(Clock::time_point now) noexcept {
        last_use = now;
        observed = true;
        retirement_failed = false;
    }
    [[nodiscard]] std::uint64_t elapsed_seconds(Clock::time_point now) const noexcept {
        if (!observed || now < last_use) return 0u;
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(now - last_use).count());
    }
    [[nodiscard]] bool expired(Clock::time_point now, std::uint32_t minutes) const noexcept {
        return observed && !retirement_failed && minutes != 0u
            && now >= last_use && now - last_use >= std::chrono::minutes{minutes};
    }
};

} // namespace epochengine::ai::native_runtime::idle
