// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "remote_nro_protocol.hpp"
#include <cstdint>
#include <string>

namespace nshome::remote {
// Per-socket framing: an idle/partial sender never owns the listener worker.
struct Session {
    static constexpr std::uint64_t TimeoutMs = 5000;
    enum class Feed { More, Line, TooLong };
    std::string line;
    std::uint64_t deadline{};
    bool authenticated{};
    void start(std::uint64_t now) { line.clear(); authenticated = false; deadline = now + TimeoutMs; }
    bool expired(std::uint64_t now) const { return now >= deadline; }
    Feed feed(char c) {
        if (c == '\n') {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            return Feed::Line;
        }
        if (line.size() >= kMaxCommandLength) return Feed::TooLong;
        line.push_back(c);
        return Feed::More;
    }
    void accept_auth(std::uint64_t now) { authenticated = true; line.clear(); deadline = now + TimeoutMs; }
};
}
