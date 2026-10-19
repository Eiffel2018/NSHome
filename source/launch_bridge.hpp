// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "codec.hpp"
#include <memory>
struct Service;
namespace observer::launch {
bool start();
void wake();
void network_tick(const char* pairing_code); // Called only by the BSD-owning 6802 worker.
void register_root(const std::shared_ptr<::Service>& forward, std::uint64_t pid, bool qlaunch);
void bind_root(const void* session, std::uint64_t pid);
// Only a PC6802-bound Start can be rejected if preselection fails.
// 6801/manual launches retain native profile selection and lifecycle.
bool intercept(const void* session, Kind kind, const Request& request, std::uint32_t& result);
void observe(const void* session, Kind kind, const Request& request, const Reply& reply);
}
