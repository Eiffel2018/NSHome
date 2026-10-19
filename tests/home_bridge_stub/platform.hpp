// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "codec.hpp"
#include <cstdint>
#include <vector>
#include <utility>
using Result = std::uint32_t;
struct Service { std::uint32_t session{}, object_id{}; bool own_handle{true}; };
inline bool serviceIsDomain(const Service* s) { return s->object_id != 0; }
inline std::uint64_t tick{};
inline std::uint64_t armGetSystemTick() { return ++tick; }
inline std::uint8_t tls[0x100];
namespace ams::sf::hipc { inline void* GetMessageBufferOnTls() { return tls; } }
namespace observer {
struct Record { Kind kind{}; std::uint32_t object{}, command{}, argument{}, result{}, flags{}; };
inline void push(Record) {}
}
inline std::vector<std::pair<std::uint32_t,std::uint32_t>> calls;
inline std::uint32_t fail_command{};
inline bool finished{};
inline Result test_call(Service* s, std::uint32_t cmd, std::uint8_t* out = nullptr) {
    calls.emplace_back(s->object_id,cmd);
    for (auto& c : tls) c = 0xee;
    if (out) *out = finished;
    return fail_command == cmd ? 0x1234 : 0;
}
#define serviceDispatch(s,c) test_call(s,c)
#define serviceDispatchOut(s,c,out) test_call(s,c,&out)
#define R_SUCCEEDED(r) ((r)==0)
#define R_FAILED(r) ((r)!=0)
#define MAKERESULT(m,d) (((d)<<9)|(m))
constexpr std::uint32_t Module_Libnx = 345, LibnxError_NotInitialized = 3;
