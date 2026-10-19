// SPDX-License-Identifier: GPL-2.0-only
#include "codec.hpp"
#include <array>
#include <cassert>
#include <cstdio>

using namespace observer;
void put(std::uint8_t* p, std::uint32_t n) { std::memcpy(p, &n, 4); }
int main() {
    std::array<std::uint8_t, 96> bytes{};
    put(bytes.data(), InMagic); put(bytes.data()+8, 100);
    assert(request(bytes.data(), 16).command == 100);
    assert(root_open(201, true, 1));
    assert(!root_open(201, false, 1));
    assert(returned_kind(Kind::Root, 201) == Kind::LibraryProxy);
    assert(returned_kind(Kind::LibraryProxy, 20) == Kind::LibrarySelf);
    assert(returned_kind(Kind::LibraryProxy, 0) == Kind::Common);
    assert(returned_kind(Kind::Proxy, 20) == Kind::Home);
    assert(returned_kind(Kind::Proxy, 23) == Kind::Global);
    assert(std::strcmp(name(Kind::Global), "global-state") == 0);
    // Power/idle-policy commands are forwarded scalar calls, not child objects.
    assert(returned_kind(Kind::Global, 2) == Kind::Unknown);
    assert(returned_kind(Kind::Global, 9) == Kind::Unknown);
    assert(returned_kind(Kind::Global, 10) == Kind::Unknown);
    std::array<std::uint8_t, 20> power_reply{};
    put(power_reply.data(), OutMagic);
    put(power_reply.data() + 16, 1);
    const auto power = reply(power_reply.data(), power_reply.size(), false, false);
    assert(power.valid && power.scalar == 1 && power.object == 0);
    auto unchanged = bytes;
    for (std::size_t n = 0; n < 16; ++n) assert(!request(bytes.data(), n).valid);
    assert(bytes == unchanged);
    bytes = {}; bytes[0] = 1; bytes[2] = 16; put(bytes.data()+4, 7);
    put(bytes.data()+16, InMagic); put(bytes.data()+24, 1);
    const auto r = request(bytes.data(), 32);
    assert(r.valid && r.domain && r.object == 7 && r.command == 1);
    bytes[1] = 1; assert(!request(bytes.data(), 32).valid); // truncated object array
    bytes[1] = 0; bytes[2] = 255; assert(!request(bytes.data(), 32).valid);
    bytes = {}; bytes[0] = 2; put(bytes.data()+4, 8);
    assert(request(bytes.data(), 16).closing);
    bytes = {}; put(bytes.data(), 1); put(bytes.data()+16, OutMagic); put(bytes.data()+32, 42);
    auto p = reply(bytes.data(), 36, true, true);
    assert(p.valid && p.object == 42 && p.result == 0);
    assert(reply(bytes.data(), 36, true, false).object == 0);
    put(bytes.data()+24, 0x1234);
    assert(reply(bytes.data(), 36, true, true).object == 0);
    assert(!reply(bytes.data(), 31, true, true).valid);
    assert(returned_kind(Kind::Root, 110) == Kind::Proxy);
    assert(returned_kind(Kind::Proxy, 0) == Kind::Common);
    assert(returned_kind(Kind::Common, 20) == Kind::Unknown);
    assert(returned_kind(Kind::Home, 20) == Kind::Storage);
    assert(returned_kind(Kind::LibCreator, 0) == Kind::Library);
    assert(returned_kind(Kind::LibCreator, 3) == Kind::Library);
    assert(root_open(110, true, 1));
    assert(root_open(100, true, 1));
    assert(!root_open(100, false, 0)); // Library PushInData is not a root proxy.
    assert(!root_open(110, false, 1));
    assert(!root_open(110, true, 0));
    assert(!root_open(110, true, 2));
    assert(!root_open(3, true, 1));
    assert(returned_kind(Kind::LibCreator, 10) == Kind::Storage);
    assert(returned_kind(Kind::Library, 104) == Kind::Storage);
    bytes = {}; put(bytes.data(), 0x564f5a42); put(bytes.data()+4, 0x83); put(bytes.data()+8, 2);
    assert(channel_header(bytes.data(), 16).message == 0x83);
    assert(channel_header(bytes.data(), 16).argument == 2);
    for (std::size_t n = 0; n < 16; ++n) assert(channel_header(bytes.data(), n).magic == 0);
    put(bytes.data(), 0x534d4153);
    assert(channel_header(bytes.data(), 16).message == 2);
    // Deterministic malformed frames: no out-of-bounds accesses and no writes.
    std::uint32_t state = 7;
    for (unsigned iteration=0; iteration<50000; ++iteration) {
        for (auto& b : bytes) { state = state*1664525u+1013904223u; b = state >> 24; }
        const auto before = bytes; const auto size = iteration % (bytes.size()+1);
        (void)request(bytes.data(), size);
        (void)reply(bytes.data(), size, iteration&1, iteration&2);
        (void)channel_header(bytes.data(), size);
        assert(bytes == before);
    }
    std::puts("codec: valid/invalid CMIF, per-interface mapping, 50000 malformed immutable frames passed");
}
