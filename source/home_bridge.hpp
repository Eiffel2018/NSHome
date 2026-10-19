// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "codec.hpp"
#include <memory>
#include <string>
#include <string_view>
struct Service;
namespace observer::home {
void register_root(const std::shared_ptr<::Service>& forward, std::uint64_t pid, bool qlaunch);
void bind_root(const void* session, std::uint64_t pid);
bool is_qlaunch(const void* session);
void observe(const void* session, Kind kind, const Request& request, const Reply& reply);
bool consume_home(const void* session);
// Authenticated prototype control. Arm only after the operator has verified
// live Breeze6801; the explicit generation binds that proof to one AM session.
std::string command(std::string_view line);
// Owned background Album accessor in the same cloned qlaunch domain.
void adopt_library(std::uint32_t object, std::uint64_t started_tick);
// Only the scoped loader receipt or authenticated identity check may call this.
bool identify_breeze(std::uint64_t generation, bool game_in_front);
}
