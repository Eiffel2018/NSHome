// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace observer {
constexpr std::uint32_t InMagic = 0x49434653, OutMagic = 0x4f434653;
enum class Kind : std::uint8_t { Unknown, Root, Proxy, Common, Home, AppCreator, LibCreator, Application, Library, Storage, StorageAccessor, LibraryProxy, LibrarySelf };
inline const char* name(Kind k) {
    constexpr const char* names[] = {"unknown", "appletAE", "system-proxy", "common-state", "home-functions", "app-creator", "lib-creator", "application", "library-applet", "general-storage", "storage-accessor", "library-proxy", "library-self"};
    return names[static_cast<unsigned>(k)];
}
inline std::uint32_t word(const void* p) { std::uint32_t n; std::memcpy(&n, p, 4); return n; }
struct ChannelHeader { std::uint32_t magic{}, message{}, argument{}; };
inline ChannelHeader channel_header(const std::uint8_t* p, std::size_t n) {
    if (n < 16) return {};
    const auto magic = word(p);
    if (magic == 0x534d4153) return {magic, word(p + 8), 0}; // SAMS
    if (magic == 0x564f5a42) return {magic, word(p + 4), word(p + 8)}; // BZOV
    return {};
}
struct Request {
    bool valid{}, domain{}, closing{};
    std::uint32_t object{}, command{}, argument{}, argument_high{}, token{};
};
// These helpers only read the already-forwarded CMIF bytes. They issue no IPC.
inline Request request(const std::uint8_t* p, std::size_t n) {
    Request r;
    if (n < 16) return r;
    if (word(p) != InMagic) {
        if (p[0] == 2) { r.valid = r.domain = r.closing = true; r.object = word(p + 4); return r; }
        const auto payload = std::uint16_t(p[2] | (p[3] << 8));
        if (p[0] != 1 || payload < 16 || payload > n - 16 || n - 16 - payload < std::size_t(p[1]) * 4) return r;
        r.domain = true; r.object = word(p + 4); p += 16; n = payload;
    }
    if (n < 16 || word(p) != InMagic) return {};
    r.valid = true; r.command = word(p + 8); r.token = word(p + 12);
    if (n >= 20) r.argument = word(p + 16);
    if (n >= 24) r.argument_high = word(p + 20);
    return r;
}
struct Reply { bool valid{}; std::uint32_t result{}, count{}, object{}, scalar{}; };
// PushInData on a library accessor is also command 100. Only an actual
// process-tagged Open*Proxy request may identify an unknown object as root.
inline bool root_open(std::uint32_t cmd, bool sends_pid, std::uint32_t copy_handles) {
    return (cmd == 100 || cmd == 110 || cmd == 200 || cmd == 201) && sends_pid && copy_handles == 1;
}
inline Reply reply(const std::uint8_t* p, std::size_t n, bool domain, bool object_reply) {
    Reply r;
    if (domain) { if (n < 32) return r; r.count = word(p); p += 16; n -= 16; }
    if (n < 16 || word(p) != OutMagic) return {};
    r.valid = true; r.result = word(p + 8);
    if (n >= 20) r.scalar = word(p + 16);
    // Every observed object getter below has zero scalar output. Other replies
    // are never guessed to contain object IDs, pointers or arbitrary payloads.
    if (object_reply && domain && r.result == 0 && r.count == 1 && n >= 20) r.object = r.scalar;
    return r;
}
inline Kind returned_kind(Kind k, std::uint32_t cmd) {
    if (k == Kind::Root && (cmd == 100 || cmd == 110)) return Kind::Proxy;
    if (k == Kind::Root && (cmd == 200 || cmd == 201)) return Kind::LibraryProxy;
    if (k == Kind::LibraryProxy) {
        switch (cmd) { case 0: return Kind::Common; case 11: return Kind::LibCreator; case 20: return Kind::LibrarySelf; }
    }
    if (k == Kind::Proxy) {
        switch (cmd) { case 0: return Kind::Common; case 11: return Kind::LibCreator; case 20: return Kind::Home; case 22: return Kind::AppCreator; }
    }
    if (k == Kind::AppCreator && (cmd == 0 || cmd == 1)) return Kind::Application;
    // Stock qlaunch on the test console also obtains this accessor via cmd3.
    // Evidence: one returned domain object, followed by accessor commands
    // 0/170/100/10 and 20/30. This does not change Atmosphere's Album override.
    if (k == Kind::LibCreator && (cmd == 0 || cmd == 3)) return Kind::Library;
    if (k == Kind::LibCreator && cmd == 10) return Kind::Storage;
    if (k == Kind::Library && (cmd == 101 || cmd == 104)) return Kind::Storage;
    if (k == Kind::Home && cmd == 20) return Kind::Storage;
    if (k == Kind::Storage && cmd == 0) return Kind::StorageAccessor;
    return Kind::Unknown;
}
}
