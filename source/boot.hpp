// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "codec.hpp"
namespace observer {
void configure_boot();
void startup_input_once();
void observe_boot(Kind kind, const Request& request, const Reply& reply);
}
