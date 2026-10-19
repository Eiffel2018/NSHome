// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace observer::background {
// Existing SwitchU/Breeze BZOV interactive-storage ABI; no Breeze binary patch.
constexpr std::uint32_t Magic = 0x564f5a42;
enum Command : std::uint32_t { EnterHidden=1, Show=2, Hide=3, EnterNormal=4, Release=5, Ack=0x81, RequestForeground=0x82, StateChanged=0x83 };
struct Message {
    std::uint32_t magic{Magic}, command{}, argument{}, reserved{};
};
static_assert(sizeof(Message) == 16);
inline bool valid(const Message& value) { return value.magic == Magic && !value.reserved; }
inline bool ack(const Message& value, Command wanted) {
    return valid(value) && value.command == Ack && value.argument == wanted;
}
inline bool capable(const char* text, std::size_t size) {
    const char* token = "overlay1";
    for (std::size_t i=0; i<size;) {
        while (i<size && (text[i]==' ' || text[i]=='\r' || text[i]=='\n')) ++i;
        const auto start=i;
        while (i<size && text[i]!=' ' && text[i]!='\r' && text[i]!='\n') ++i;
        if (i-start==8 && !std::memcmp(text+start,token,8)) return true;
    }
    return false;
}
}
