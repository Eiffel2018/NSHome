// SPDX-License-Identifier: GPL-2.0-only
#include "home_bridge.hpp"
#include "platform.hpp"
#include <array>
#include <cassert>
#include <cstring>
using namespace observer;
int main() {
    int session{}, unrelated{};
    auto q = std::make_shared<Service>();
    q->object_id = 1;
    home::register_root(q, 100, true);
    home::bind_root(&session, 100);
    auto seen = [&](Kind kind, unsigned cmd, unsigned object, unsigned child = 0, bool closing = false) {
        Request request; request.valid = request.domain = true;
        request.command = cmd; request.object = object; request.closing = closing;
        Reply reply; reply.valid = true; reply.object = child;
        home::observe(&session, kind, request, reply);
    };
    seen(Kind::Proxy, 20, 2, 10);
    seen(Kind::AppCreator, 0, 14, 40);
    seen(Kind::LibCreator, 3, 16, 50);
    auto b = std::make_shared<Service>();
    home::register_root(b, 133, false); // root can connect before Start returns
    seen(Kind::Library, 10, 50);
    assert(home::command("home arm 2").find("armed=2") != std::string::npos);
    assert(!home::consume_home(&unrelated));
    assert(home::command("home arm 2").find("armed=2") != std::string::npos);
    std::array<std::uint8_t, 0x100> saved{};
    for (unsigned i=0; i<saved.size(); ++i) tls[i] = saved[i] = i;
    assert(home::consume_home(&session));
    assert(std::memcmp(tls, saved.data(), saved.size()) == 0);
    assert((calls == std::vector<std::pair<unsigned,unsigned>>{{50,1},{40,1},{10,12},{40,101}}));
    calls.clear();
    assert(home::consume_home(&session));
    assert((calls == std::vector<std::pair<unsigned,unsigned>>{{50,1},{40,1},{10,10}}));
    fail_command = 101;
    assert(!home::consume_home(&session));
    assert(home::command("home status").find("armed=0") != std::string::npos);
    assert(std::memcmp(tls, saved.data(), saved.size()) == 0);
    fail_command = 0;
    assert(home::command("home arm 2").find("armed=2") != std::string::npos);
    finished = true;
    assert(!home::consume_home(&session));
    finished = false;
    assert(home::command("home arm 2").find("armed=2") != std::string::npos);
    b.reset(); // NRO exit/chainload drops this exact AM root generation.
    assert(!home::consume_home(&session));
    auto next = std::make_shared<Service>();
    home::register_root(next, 133, false); // same PID, new session is not armed
    assert(home::command("home arm 2") == "ERR home-not-ready\n");
    assert(home::command("home arm 3").find("armed=3") != std::string::npos);
    seen(Kind::Library, 0, 50, 0, true);
    assert(!home::consume_home(&session));
    assert(home::command("home arm 3") == "ERR home-not-ready\n");
    assert(home::command("home arm -1") == "ERR home-generation\n");
    assert(home::command("home arm 99999999999999999999") == "ERR home-generation\n");
    assert(home::command("home off") == "OK home disabled\n");
}
