// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <atomic>
#include <cstdint>
namespace observer {
class BootInputGate {
    std::atomic<std::uint64_t> ready{};
    std::atomic<bool> locked{}, cancelled{}, claimed{};
public:
    void lock() { locked.store(true); }
    void foreground(std::uint64_t ms) { std::uint64_t empty = 0; ready.compare_exchange_strong(empty, ms + 1); }
    void cancel() { cancelled.store(true); }
    bool window(std::uint64_t ms) const {
        const auto r = ready.load();
        return r && locked.load() && !cancelled.load() && ms >= r - 1 + 1000 && ms <= r - 1 + 5000;
    }
    bool claim(std::uint64_t ms) { bool empty = false; return window(ms) && claimed.compare_exchange_strong(empty, true); }
    bool may_send(std::uint64_t ms) const { return claimed.load() && window(ms); }
};
}
