// SPDX-License-Identifier: GPL-2.0-only
#include "boot_gate.hpp"
#include "boot_screen.hpp"
#include <array>
#include <cassert>
int main() {
    assert(!observer::boot_prompt_image(nullptr, 1280, 720));
    observer::BootInputGate gate;
    assert(!gate.claim(30000)); // No qlaunch foreground, never send.
    gate.foreground(1000);
    assert(!gate.claim(2000)); // Foreground alone does not prove boot lock.
    gate.lock();
    assert(!gate.claim(1999));
    assert(gate.claim(2000));
    assert(!gate.claim(2000)); // Exactly one attempt.
    assert(gate.may_send(3250));
    gate.cancel(); // Manual unlock during controller discovery.
    assert(!gate.may_send(3251));
    observer::BootInputGate sleeping;
    sleeping.lock();
    sleeping.foreground(1000);
    sleeping.cancel();
    sleeping.foreground(60000); // Wake cannot extend/rearm the boot window.
    assert(!sleeping.claim(61000));
    observer::BootInputGate expired;
    expired.lock();
    expired.foreground(1000);
    expired.foreground(50000);
    assert(!expired.claim(6001));
    assert(!expired.claim(51000));
}
