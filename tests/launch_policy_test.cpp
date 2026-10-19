// SPDX-License-Identifier: GPL-2.0-only
#include "launch_policy.hpp"
#include <cassert>
#include <cstdio>
using namespace observer::launch;
int main() {
    const User user{{0x1122334455667788, 0x8877665544332211}};
    // No PC intent: a manual/native application start cannot schedule a return.
    Policy manual;
    assert(!manual.bind(0x1234, 1, 40)); manual.started(40, 0);
    assert(manual.phase == Phase::Idle && !manual.due(999999));
    assert(!manual.begin(Origin::Remote6802, 0, 0x1234, 1, user, 0));
    Policy unsent;
    assert(unsent.begin(Origin::Remote6802, 7, 0x1234, 1, user, 0));
    assert(!unsent.bind(0x1234, 1, 40)); // a same-title manual start before submission
    assert(unsent.phase == Phase::Cancelled);
    for (const auto origin : {Origin::Remote6802}) {
        Policy p;
        assert(!p.begin(origin, 1, 0x1234, 2, {}, 0));
        assert(p.begin(origin, 1, 0x1234, 2, user, 100));
        assert(!p.begin(origin, 2, 0x4567, 2, user, 150));
        assert(!p.due(999999)); // request acknowledgement is not application Start
        assert(!p.submit(2, 2)); assert(!p.submit(1, 3));
        assert(p.submit(1, 2)); assert(!p.submit(1, 2));
        assert(p.bind(0x1234, 2, 40));
        assert(p.bind(0x1234, 2, 41)); // clean accessor before Start
        p.started(40, 200); assert(p.phase == Phase::Requested);
        p.started(41, 200); p.started(41, 9999);
        assert(!p.due(2199)); assert(p.due(2200));
        p.retry(2200); assert(!p.due(4199)); assert(p.due(4200));
        p.restoring(); assert(!p.due(4200));
        p.ready(3, 8); assert(p.phase == Phase::Restoring);
        p.ready(2, 0); assert(p.phase == Phase::Restoring);
        p.retry_restore(4500); assert(!p.due(6499)); assert(p.due(6500));
        p.restoring(); p.ready(2, 8); assert(p.phase == Phase::Ready && p.generation == 8);
        assert(!p.pending());
        p.started(41, 10000); assert(p.phase == Phase::Ready && !p.due(999999));
        assert(p.begin(origin, 2, 0x1234, 2, user, 10000));
        assert(p.submit(2, 2));
        assert(!p.bind(0x4567, 2, 42)); assert(p.phase == Phase::Cancelled);
        assert(p.begin(origin, 3, 0x1234, 2, user, 11000));
        p.check(3, 11001); assert(p.phase == Phase::Cancelled); // qlaunch replaced
        assert(p.begin(origin, 4, 0x1234, 2, user, 12000));
        p.check(2, 11999); assert(p.phase == Phase::Cancelled); // clock reset
        assert(p.begin(origin, 5, 0x1234, 2, user, 13000));
        p.check(2, 43000); assert(p.phase == Phase::Cancelled); // bounded wait
        assert(p.begin(origin, 6, 0x1234, 2, user, 44000));
        p.fail(0x1234); assert(p.phase == Phase::Failed && p.result == 0x1234);
    }
    assert(is_breeze_state("+OK {\"breeze\":\"beta125.05\",\"menu\":{}}"));
    assert(is_breeze_state("+OK {\"breeze\": \"beta125.05\"}"));
    for (const auto invalid : {"-ERR busy", "+OK {}", "+OK {\"menu\":{\"breeze\":\"beta125.05\"}}",
            "+OK {\"breeze\":\"beta\"}", "+OK {\"breeze\":\"beta125\\n\"}",
            "+OK {\"breeze\":\"beta125.05", "+OK {\"breeze\":\"beta125.05\"x}"}) assert(!is_breeze_state(invalid));
    std::puts("launch policy, source fencing and Breeze identity tests passed");
}
