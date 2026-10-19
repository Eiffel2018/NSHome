// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstdint>
namespace observer::launch {
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
