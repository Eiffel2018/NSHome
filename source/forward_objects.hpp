// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "codec.hpp"
#include <array>
namespace observer {
class ForwardObjects {
    struct Object { const void* session{}; std::uint32_t id{}; Kind kind{}; };
    struct Session { const void* address{}; std::uint64_t generation{}; };
    std::array<Object,2048> objects{};
    std::array<Session,32> sessions{};
public:
    void begin(const void* address, std::uint64_t generation) {
        Session* slot = nullptr;
        for (auto& entry : sessions) if (entry.address == address) { slot = &entry; break; }
        if (slot && slot->generation == generation) return;
        for (auto& object : objects) if (object.session == address) object = {};
        if (!slot) for (auto& entry : sessions) if (!entry.address) { slot = &entry; break; }
        if (!slot) slot = &sessions[0]; // Unknown classification fails closed.
        *slot = {address,generation};
    }
    Kind kind(const void* session, std::uint32_t id, bool root_open) const {
        if (root_open) return Kind::Root;
        for (const auto& object : objects) if (object.session == session && object.id == id) return object.kind;
        return Kind::Unknown;
    }
    void remember(const void* session, std::uint32_t id, Kind kind) {
        for (auto& object : objects) if ((object.session == session && object.id == id) || !object.session) {
            object = {session,id,kind}; return;
        }
    }
    void forget(const void* session, std::uint32_t id) {
        for (auto& object : objects) if (object.session == session && object.id == id) object = {};
    }
};
}
