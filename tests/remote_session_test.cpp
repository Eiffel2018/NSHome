// SPDX-License-Identifier: GPL-2.0-only
#include "remote_session.hpp"
#include <cassert>
using namespace nshome::remote;
int main() {
    Session idle, active;
    idle.start(0); active.start(10);
    for (char c : std::string("auth code\r\nstatus\n")) {
        auto result = active.feed(c);
        if (result != Session::Feed::Line) continue;
        if (!active.authenticated) {
            assert(active.line == "auth code"); active.accept_auth(20);
        } else assert(active.line == "status");
    }
    assert(active.authenticated && active.line == "status");
    assert(idle.line.empty() && !idle.authenticated);
    assert(!idle.expired(4999) && idle.expired(5000));
    assert(!active.expired(5019) && active.expired(5020));
    Session slow; slow.start(100);
    for (unsigned i=0; i<kMaxCommandLength; ++i) assert(slow.feed('x') == Session::Feed::More);
    assert(slow.feed('x') == Session::Feed::TooLong);
    assert(slow.expired(5100)); // Bytes do not extend the phase deadline.
}
