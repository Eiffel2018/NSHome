// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "codec.hpp"
#include <memory>
struct Service;
namespace observer::launch {
void request_stock_home(const void* session);
bool start();
void wake();
void network_tick(const char* pairing_code);
void network_reset(); // Called only by the BSD-owning 6802 worker.
void register_root(const std::shared_ptr<::Service>& forward, std::uint64_t pid, bool qlaunch);
std::uint64_t bind_root(const void* session, std::uint64_t pid);
// Observe launch provenance without replacing qlaunch's application commands.
void before_forward(const void* session, Kind kind, const Request& request);
void observe(const void* session, Kind kind, const Request& request, const Reply& reply);
}
