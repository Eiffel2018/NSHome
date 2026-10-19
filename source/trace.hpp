// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <stratosphere.hpp>
#include "codec.hpp"
namespace observer {
struct Record {
    std::uint64_t tick{}, seq{}, session{}, pid{};
    std::uint32_t object{}, command{}, argument{}, transport{}, result{}, scalar{}, child{}, flags{};
    Kind kind{};
};
void push(Record record);
void start_log();
void accepted(std::uint64_t pid);
void startup(std::uint32_t stage, std::uint32_t result = 0, std::uint32_t value = 0);
}
