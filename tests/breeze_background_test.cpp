// SPDX-License-Identifier: GPL-2.0-only
#include "breeze_background.hpp"
#include <cassert>
using namespace observer::background;
int main() {
    assert(capable("overlay1 overlay2",17));
    assert(capable("\r\n overlay1\n",12));
    assert(!capable("overlay10",9));
    assert(!capable("xoverlay1",9));
    assert(!capable("overlay",7));
    Message response{Magic,Ack,EnterHidden,0};
    assert(ack(response,EnterHidden));
    assert(!ack(response,EnterNormal));
    response.command=StateChanged; assert(!ack(response,EnterHidden));
    response.command=Ack; response.magic=0; assert(!ack(response,EnterHidden));
    response.magic=Magic; response.reserved=1; assert(!ack(response,EnterHidden));
}
