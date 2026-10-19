// SPDX-License-Identifier: GPL-2.0-only
// Adapted from Eiffel2018/SwitchU 546903f6 (see NOTICE.md).
#pragma once

#include <switch.h>

namespace nshome::remote_input {

// Creates a temporary virtual controller for one bounded input pulse and
// tears down all owned HDLS resources before returning. The returned libnx
// Result is the first failure, or 0.
// Accepts up to four ordinary buttons together, or one HOME/Capture/rail button.
Result pressButton(u64 button, u32 holdMs);
// Boot-only pulse: recheck after controller discovery and cancel without a
// pressed sample if HOME has already unlocked. attempted is never replayed.
Result pressButtonIf(u64 button, u32 holdMs, bool (*allowed)(), bool* attempted);

// Bounded analog impulse; positive Y is up. Both axes return to center before
// replying, including a best-effort neutral update on a failed state call.
Result moveStick(bool right, s32 x, s32 y, u32 holdMs);

// Releases all buttons and destroys only this module's virtual controller.
Result shutdown();

} // namespace nshome::remote_input
