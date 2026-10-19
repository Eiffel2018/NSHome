// SPDX-License-Identifier: GPL-2.0-only
// Adapted from Eiffel2018/SwitchU 546903f6 (see NOTICE.md).
#pragma once
#include <switch.h>

namespace nshome::remote_touch {
// Single-finger screen gesture, with explicit lift and autopilot teardown.
// Coordinates use the 1280x720 capture; no virtual gamepad is attached.
Result gesture(u32 x, u32 y, u32 holdMs, bool twice, u32 gapMs);
Result gestureIf(u32 x, u32 y, u32 holdMs, bool (*allowed)(), bool* attempted);
Result shutdown();
}
