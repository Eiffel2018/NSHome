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
    assert(home::is_qlaunch(&session));
    assert(!home::is_qlaunch(&unrelated));
    assert(!home::is_qlaunch(nullptr));
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
    // Only a proved, newly initialized NRO can arm the owned background accessor.
    home::adopt_library(80, armGetSystemTick());
    assert(!home::identify_breeze(3, true)); // old Album predates the owned library
    auto restored = std::make_shared<Service>();
    home::register_root(restored, 150, false);
    assert(!home::identify_breeze(3, true));
    assert(home::identify_breeze(4, true));
    calls.clear();
    assert(home::consume_home(&session));
    assert((calls == std::vector<std::pair<unsigned,unsigned>>{{80,1},{40,1},{10,10}}));
    assert(home::identify_breeze(4, true, true));
    calls.clear();
    assert(home::consume_home(&session));
    assert(calls.empty()); // Relay must return before waiting for Breeze's IPC.
    home::Toggle toggle, duplicate;
    assert(home::take_toggle(toggle));
    assert(toggle.game_front && toggle.album == 4 && toggle.library == 80);
    assert(home::consume_home(&session)); // Coalesce, do not submit twice.
    assert(!home::take_toggle(duplicate));
    assert(home::matches(toggle));
    home::finish_toggle(toggle,true);
    assert(!home::matches(toggle));
    assert(home::background_pair(duplicate) && !duplicate.game_front);
    home::request_foreground(); // Already in applet foreground: no new request.
    assert(!home::take_toggle(duplicate));
    assert(home::consume_home(&session));
    assert(home::take_toggle(duplicate) && !duplicate.game_front);
    home::finish_toggle(duplicate,true);
    home::request_foreground(); // Breeze requests a native keyboard/normal UI.
    assert(home::take_toggle(toggle) && toggle.game_front);
    home::command("home off");
    assert(!home::matches(toggle));
    home::finish_toggle(toggle,true);
    assert(home::command("home status").find("armed=0") != std::string::npos);
    assert(home::identify_breeze(4,true,true));
    assert(home::consume_home(&session));
    assert(home::take_toggle(toggle));
    home::finish_toggle(toggle,false); // Unknown Ack must not replay HOME.
    assert(!home::take_toggle(toggle));
    assert(!home::consume_home(&session));
    assert(home::identify_breeze(4,true,true));
    assert(home::consume_home(&session));
    assert(home::take_toggle(toggle));
    restored.reset();
    assert(!home::matches(toggle)); // Holder exit invalidates an in-flight Ack.
    assert(!home::consume_home(&session));
    // A replacement request follows the Album process through NRO->hbmenu,
    // but must not terminate a new process or a reused native accessor.
    auto first = std::make_shared<Service>();
    home::register_root(first,900,false);
    home::adopt_library(90,1000);
    home::LibrarySnapshot snapshot;
    assert(home::library_snapshot(snapshot));
    assert(snapshot.pid==900 && snapshot.object==90);
    assert(home::matches_library(snapshot));
    auto next_nro = std::make_shared<Service>();
    home::register_root(next_nro,900,false);
    assert(home::matches_library(snapshot));
    auto foreign = std::make_shared<Service>();
    home::register_root(foreign,901,false);
    assert(!home::matches_library(snapshot));
    assert(home::library_snapshot(snapshot));
    home::adopt_library(91,1001);
    assert(!home::matches_library(snapshot));
}
