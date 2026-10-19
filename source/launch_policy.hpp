// SPDX-License-Identifier: GPL-2.0-only
// Timing/source policy adapted from SwitchU 546903f6, breeze_launch_handoff.hpp.
#pragma once
#include <cstdint>
#include <string_view>

namespace observer::launch {
struct User { std::uint64_t words[2]{}; bool valid() const { return words[0] || words[1]; } };
enum class Origin { Remote6802 };
enum class Phase { Idle, Requested, Started, Restoring, Ready, Failed, Cancelled };

// One launch and one return, fenced by the qlaunch session and actual accessor.
// Repeated observations never restart the two-second delay or submit twice.
class Policy {
public:
    static constexpr std::uint64_t Delay = 2000, Timeout = 30000, Retry = 2000;
    Phase phase{Phase::Idle};
    Origin origin{};
    std::uint64_t id{}, title{}, epoch{}, since{}, next{}, generation{};
    std::uint32_t application{}, result{};
    bool submitted{};
    User user{};
    bool pending() const { return phase == Phase::Requested || phase == Phase::Started || phase == Phase::Restoring; }
    bool begin(Origin source, std::uint64_t request, std::uint64_t tid, std::uint64_t session,
               User uid, std::uint64_t now) {
        if (pending() || !request || !tid || !session || !uid.valid()) return false;
        *this = {};
        origin = source; id = request; title = tid; epoch = session; user = uid;
        since = now; phase = Phase::Requested;
        return true;
    }
    bool bind(std::uint64_t tid, std::uint64_t session, std::uint32_t accessor) {
        if (phase != Phase::Requested) return false;
        if (!submitted || tid != title || session != epoch || !accessor) { cancel(); return false; }
        // Stock qlaunch may replace a popped accessor with a clean accessor
        // before Start. The exact title/epoch must still match.
        application = accessor;
        return true;
    }
    bool submit(std::uint64_t request, std::uint64_t current) {
        if (phase != Phase::Requested || submitted || id != request || epoch != current) return false;
        submitted = true; return true;
    }
    void started(std::uint32_t accessor, std::uint64_t now) {
        if (phase != Phase::Requested || !application || accessor != application) return;
        phase = Phase::Started; since = now; next = now + Delay;
    }
    bool due(std::uint64_t now) const { return phase == Phase::Started && now >= next; }
    void retry(std::uint64_t now) { if (phase == Phase::Started) next = now + Retry; }
    void retry_restore(std::uint64_t now) { if (phase == Phase::Restoring) { phase = Phase::Started; next = now + Retry; } }
    void restoring() { if (phase == Phase::Started) phase = Phase::Restoring; }
    void ready(std::uint64_t current, std::uint64_t album, bool endpoint_ready) {
        if ((phase == Phase::Started || phase == Phase::Restoring) && current == epoch && album && endpoint_ready) {
            phase = Phase::Ready; generation = album;
        }
    }
    void fail(std::uint32_t rc) { if (pending()) { phase = Phase::Failed; result = rc; } }
    void cancel() { if (pending()) phase = Phase::Cancelled; }
    void check(std::uint64_t current, std::uint64_t now) {
        if (pending() && (epoch != current || now < since || now - since >= Timeout)) cancel();
    }
};

inline bool is_breeze_state(std::string_view reply) {
    constexpr std::string_view prefix = "+OK {\"breeze\":";
    if (reply.substr(0, prefix.size()) != prefix) return false;
    reply.remove_prefix(prefix.size());
    if (!reply.empty() && reply.front() == ' ') reply.remove_prefix(1);
    if (reply.substr(0, 5) != "\"beta") return false;
    reply.remove_prefix(5);
    unsigned digits = 0;
    for (std::size_t i = 0; i < reply.size() && i < 64; ++i) {
        const char c = reply[i];
        if (c >= '0' && c <= '9') { ++digits; continue; }
        if (c == '.' || c == '-' || c == '_') continue;
        if (c == '"') return digits && i + 1 < reply.size() && (reply[i + 1] == ',' || reply[i + 1] == '}');
        return false;
    }
    return false;
}
}
