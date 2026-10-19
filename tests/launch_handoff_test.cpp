// SPDX-License-Identifier: GPL-2.0-only
#include "launch_handoff.hpp"
#include <cassert>
using namespace observer::launch;
int main() {
    NativeHandoff native;
    NativeIntent intent;
    native.request(1,10,NativeReason::Home);
    assert(native.take(1,10,false,intent) && intent.reason==NativeReason::Home);
    assert(!native.take(1,10,false,intent)); // one native handoff only
    native.request(1,10,NativeReason::Launch);
    native.request(1,10,NativeReason::Home);
    assert(native.take(1,10,false,intent) && intent.reason==NativeReason::Launch);
    native.request(1,10,NativeReason::Launch);
    assert(!native.take(2,10,false,intent)); // qlaunch replaced
    native.request(1,10,NativeReason::Home);
    assert(!native.take(1,11,false,intent)); // a different NRO process
    native.request(1,10,NativeReason::Launch);
    assert(!native.take(1,10,true,intent)); // explicit PC transaction owns its flow
    const HandoffSnapshot home{1,0,false,false,true};
    Handoff cold(home,0);assert(cold.next(home,1)==HandoffAction::Ready);
    HandoffSnapshot breeze{1,2,true,true,false};
    Handoff warm(breeze,100);
    assert(warm.next(breeze,100)==HandoffAction::Home);
    assert(warm.next(breeze,101)==HandoffAction::Wait); // never replay HOME
    HandoffSnapshot closing{1,0,false,true,true};
    assert(warm.next(closing,102)==HandoffAction::Wait); // wait for native slot release
    assert(warm.next(home,103)==HandoffAction::Ready);
    Handoff waiting(breeze,100);
    assert(waiting.next(breeze,100)==HandoffAction::Home);
    assert(waiting.next(breeze,5099)==HandoffAction::Wait);
    assert(waiting.next(breeze,5100)==HandoffAction::TimedOut);
    HandoffSnapshot foreign=breeze;foreign.identified=false;
    Handoff unknown(foreign,0);assert(unknown.next(foreign,0)==HandoffAction::Busy);
    Handoff changed(breeze,0);assert(changed.next(breeze,0)==HandoffAction::Home);
    auto other=breeze;other.album=3;assert(changed.next(other,1)==HandoffAction::Cancelled);
    other=breeze;other.epoch=2;assert(changed.next(other,1)==HandoffAction::Cancelled);
    Handoff reset(breeze,100);assert(reset.next(breeze,99)==HandoffAction::Cancelled);
    auto modal=home;modal.library_active=true;
    Handoff blocked(modal,0);assert(blocked.next(modal,0)==HandoffAction::Busy);
}
