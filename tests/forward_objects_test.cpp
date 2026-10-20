// SPDX-License-Identifier: GPL-2.0-only
#include "forward_objects.hpp"
#include <cassert>
using namespace observer;
int main() {
    static ForwardObjects objects;
    int address{}, other{};
    objects.begin(&address,1); objects.begin(&other,2);
    objects.remember(&address,40,Kind::LibCreator);
    objects.remember(&address,41,Kind::Common);
    objects.remember(&other,40,Kind::Application);
    objects.begin(&address,1);
    assert(objects.kind(&address,41,false) == Kind::Common);
    assert(objects.kind(&address,40,true) == Kind::Root);
    objects.begin(&address,3); // Same pool address and object IDs, new AM root.
    assert(objects.kind(&address,40,false) == Kind::Unknown);
    assert(objects.kind(&address,41,false) == Kind::Unknown);
    assert(objects.kind(&other,40,false) == Kind::Application);
    objects.remember(&address,40,Kind::Root);
    objects.forget(&address,40);
    assert(objects.kind(&address,40,false) == Kind::Unknown);
}
