// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "remote_nro_protocol.hpp"
#include <string>
namespace nshome::titles {
bool start(std::uint64_t boot);
std::string command(const remote::Command& request);
}
