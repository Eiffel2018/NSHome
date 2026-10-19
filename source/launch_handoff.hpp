// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstdint>
namespace observer::launch {
enum class NativeReason { Home, Launch };
struct NativeIntent {
    std::uint64_t epoch{}, pid{};
    NativeReason reason{};
};
// Stock HOME/launch requests supersede only the same retained Album process.
class NativeHandoff {
    NativeIntent intent_{};
public:
    void request(std::uint64_t epoch, std::uint64_t pid, NativeReason reason) {
        if (!epoch || !pid) return;
        if (intent_.epoch == epoch && intent_.pid == pid && intent_.reason == NativeReason::Launch)
            return; // HOME must not replace an already queued native launch.
        intent_ = {epoch, pid, reason};
    }
    bool take(std::uint64_t epoch, std::uint64_t pid, bool pc_pending, NativeIntent& out) {
        const auto value = intent_;
        intent_ = {};
        if (!value.epoch || value.epoch != epoch || value.pid != pid || pc_pending) return false;
        out = value;
        return true;
    }
};
struct HandoffSnapshot {
    std::uint64_t epoch{}, album{};
    bool identified{}, library_active{}, qlaunch_in_focus{};
};
enum class HandoffAction { Ready, Home, Wait, Busy, Cancelled, TimedOut };
class Handoff {
    std::uint64_t epoch_, album_, since_;
    bool sent_{};
public:
    static constexpr std::uint64_t Timeout = 5000;
    Handoff(HandoffSnapshot initial, std::uint64_t now)
        : epoch_(initial.epoch), album_(initial.album), since_(now) {}
    HandoffAction next(HandoffSnapshot current, std::uint64_t now) {
        if (current.epoch != epoch_ || now < since_) return HandoffAction::Cancelled;
        if (current.album && (current.album != album_ || !current.identified))
            return sent_ ? HandoffAction::Cancelled : HandoffAction::Busy;
        if (!album_) return current.library_active || !current.qlaunch_in_focus
            ? HandoffAction::Busy : HandoffAction::Ready;
        if (!current.album && !current.library_active && current.qlaunch_in_focus)
            return HandoffAction::Ready;
        if (now - since_ >= Timeout) return HandoffAction::TimedOut;
        if (current.album && !sent_) { sent_ = true; return HandoffAction::Home; }
        return HandoffAction::Wait;
    }
};
}
