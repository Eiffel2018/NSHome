// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstdint>
namespace observer {
void start_pm_stop();
namespace launch {
bool native_home_ready();
// PM service thread only; the AM thread must remain free while Close runs.
bool compatibility_stop(std::uint64_t caller, std::uint64_t target, std::uint32_t& result);
}
}
