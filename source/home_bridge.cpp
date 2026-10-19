// SPDX-License-Identifier: GPL-2.0-only
#include "home_bridge.hpp"
#ifdef NSH_BRIDGE_HOST_TEST
#include "platform.hpp"
#else
#include "trace.hpp"
#endif
#include <array>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace observer::home {
namespace {
struct Root {
    std::weak_ptr<::Service> forward;
    std::uint64_t pid{}, generation{}, tick{};
    bool qlaunch{};
};
std::array<Root, 16> roots;
std::mutex mutex;
std::uint64_t serial{}, qgeneration{}, armed_generation{};
const void* qsession{};
std::uint32_t library{}, application{}, functions{};
std::uint64_t library_start{}, library_created{};
bool game_front{};
std::uint64_t background_generation{}, toggle_sequence{};
bool toggle_pending{}, toggle_taken{};

Root* find(std::uint64_t generation) {
    for (auto& r : roots) if (r.generation == generation) return &r;
    return nullptr;
}
Root* album() {
    Root* latest = nullptr;
    for (auto& r : roots)
        if (!r.qlaunch && !r.forward.expired() && (!latest || r.generation > latest->generation)) latest = &r;
    return latest;
}
bool valid() {
    const auto* b = find(armed_generation);
    const auto* q = find(qgeneration);
    return armed_generation && b && q && !b->forward.expired() && !q->forward.expired()
        && library && application && functions && library_start && b->tick >= library_created;
}
Toggle pair() {
    return {qgeneration, armed_generation, toggle_sequence, library, application, functions, game_front};
}
bool same(const Toggle& t) {
    return valid() && background_generation == armed_generation && t.qlaunch == qgeneration &&
        t.album == armed_generation && t.sequence == toggle_sequence && t.library == library &&
        t.application == application && t.functions == functions && t.game_front == game_front;
}
struct PreserveTls {
    std::array<std::uint8_t, 0x100> bytes;
    PreserveTls() { std::memcpy(bytes.data(), ams::sf::hipc::GetMessageBufferOnTls(), bytes.size()); }
    ~PreserveTls() { std::memcpy(ams::sf::hipc::GetMessageBufferOnTls(), bytes.data(), bytes.size()); }
};
::Result call(const std::shared_ptr<::Service>& root, std::uint32_t object, std::uint32_t cmd) {
    ::Service service = *root;
    service.object_id = object;
    service.own_handle = false;
    return serviceDispatch(&service, cmd);
}
::Result completed(const std::shared_ptr<::Service>& root, std::uint32_t object, bool& done) {
    ::Service service = *root;
    service.object_id = object;
    service.own_handle = false;
    std::uint8_t value = 1;
    const auto rc = serviceDispatchOut(&service, 1, value);
    done = value != 0;
    return rc;
}
}
void register_root(const std::shared_ptr<::Service>& forward, std::uint64_t pid, bool qlaunch) {
    std::scoped_lock lock(mutex);
    for (auto& r : roots) if (r.forward.expired()) {
        r = {forward, pid, ++serial, armGetSystemTick(), qlaunch};
        if (qlaunch) { qgeneration = r.generation; qsession = nullptr; library = application = functions = 0; armed_generation = 0; }
        return;
    }
}
void bind_root(const void* session, std::uint64_t pid) {
    std::scoped_lock lock(mutex);
    auto* q = find(qgeneration);
    if (q && q->pid == pid && !q->forward.expired()) qsession = session;
}
bool is_qlaunch(const void* session) {
    std::scoped_lock lock(mutex);
    const auto* q = find(qgeneration);
    return session && session == qsession && q && !q->forward.expired();
}
void observe(const void* session, Kind kind, const Request& request, const Reply& reply) {
    std::scoped_lock lock(mutex);
    if (session != qsession) return;
    if (!valid()) armed_generation = 0;
    if (request.closing) {
        if (request.object == library) { library = 0; library_start = 0; armed_generation = 0; }
        if (request.object == application) { application = 0; armed_generation = 0; }
        if (request.object == functions) { functions = 0; armed_generation = 0; }
        return;
    }
    if (!reply.valid || reply.result) return;
    const auto child = returned_kind(kind, request.command);
    if (reply.object && child == Kind::Library) { library = reply.object; library_start = 0; library_created = armGetSystemTick(); armed_generation = 0; }
    if (reply.object && child == Kind::Application) { application = reply.object; armed_generation = 0; }
    if (reply.object && child == Kind::Home) functions = reply.object;
    if (kind == Kind::Library && request.object == library) {
        if (request.command == 10) library_start = armGetSystemTick();
        if (request.command == 20 || request.command == 25 || request.command == 30 || (request.command == 1 && reply.scalar)) armed_generation = 0;
    }
    if (kind == Kind::Application && request.object == application &&
        (request.command == 20 || request.command == 25 || request.command == 30 || (request.command == 1 && reply.scalar))) armed_generation = 0;
}
bool consume_home(const void* session) {
    std::scoped_lock lock(mutex);
    if (session != qsession || !valid()) { armed_generation = 0; return false; }
    if (background_generation == armed_generation) {
        if (!toggle_pending) { ++toggle_sequence; toggle_pending = true; toggle_taken = false; }
        return true; // Reply first; the worker then waits for the applet's Ack.
    }
    auto root = find(qgeneration)->forward.lock();
    if (!root || !serviceIsDomain(root.get())) { armed_generation = 0; return false; }
    // Extra IPC uses only the relay thread, after its original response is
    // complete. Preserve that response, including its original HIPC header.
    PreserveTls saved;
    bool library_done = true, application_done = true;
    ::Result rc = completed(root, library, library_done);
    if (R_SUCCEEDED(rc)) rc = completed(root, application, application_done);
    if (R_SUCCEEDED(rc) && !library_done && !application_done) {
        if (!game_front) {
            rc = call(root, functions, 12); // UnlockForeground
            if (R_SUCCEEDED(rc)) rc = call(root, application, 101);
        } else {
            rc = call(root, functions, 10); // RequestToGetForeground, retained child
        }
    } else if (R_SUCCEEDED(rc)) rc = MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    Record r;
    r.kind = Kind::Library; r.object = library; r.command = game_front ? 10 : 101;
    r.argument = application; r.result = rc; r.flags = 0x800;
    push(r);
    if (R_FAILED(rc)) { armed_generation = 0; return false; }
    game_front = !game_front;
    return true;
}
std::string command(std::string_view line) {
    std::scoped_lock lock(mutex);
    if (!valid()) armed_generation = 0;
    if (line == "home off") { armed_generation = 0; toggle_pending = toggle_taken = false; ++toggle_sequence; return "OK home disabled\n"; }
    if (line.substr(0, 9) == "home arm ") {
        const auto value = line.substr(9);
        std::uint64_t generation = 0;
        if (value.empty() || value.size() > 19) return "ERR home-generation\n";
        for (const char c : value) {
            if (c < '0' || c > '9') return "ERR home-generation\n";
            generation = generation * 10 + static_cast<unsigned>(c - '0');
        }
        auto* b = album();
        if (!b || b->generation != generation || !qsession || !library || !application || !functions || !library_start || b->tick < library_created)
            return "ERR home-not-ready\n";
        armed_generation = generation;
        game_front = false; // Operator arms only with observed Breeze in front.
        if (!valid()) { armed_generation = 0; return "ERR home-not-ready\n"; }
    } else if (line != "home status") return "ERR home-command\n";
    const auto* b = album();
    char out[256];
    std::snprintf(out, sizeof(out), "OK home armed=%llu album=%llu pid=%llu library=%u game=%u functions=%u game_front=%u\n",
        (unsigned long long)armed_generation, (unsigned long long)(b ? b->generation : 0),
        (unsigned long long)(b ? b->pid : 0), library, application, functions, game_front);
    return out;
}
void adopt_library(std::uint32_t object, std::uint64_t tick) {
    std::scoped_lock lock(mutex);
    library = object; library_created = library_start = tick; armed_generation = 0;
}
bool identify_breeze(std::uint64_t generation, bool game_in_front, bool background) {
    std::scoped_lock lock(mutex);
    const auto* b = album();
    if (!b || b->generation != generation) return false;
    armed_generation = generation;
    if (!valid()) { armed_generation = 0; return false; }
    game_front = game_in_front;
    background_generation = background ? generation : 0;
    toggle_pending = toggle_taken = false; ++toggle_sequence;
    return true;
}
bool background_pair(Toggle& out) {
    std::scoped_lock lock(mutex);
    if (!valid() || background_generation != armed_generation) return false;
    out = pair(); return true;
}
bool take_toggle(Toggle& out) {
    std::scoped_lock lock(mutex);
    if (!valid() || background_generation != armed_generation || !toggle_pending || toggle_taken) return false;
    out = pair(); toggle_taken = true; return true;
}
bool matches(const Toggle& value) { std::scoped_lock lock(mutex); return same(value); }
void finish_toggle(const Toggle& value, bool success) {
    std::scoped_lock lock(mutex);
    if (!same(value)) return;
    if (success) game_front = !game_front;
    else armed_generation = 0; // Do not replay an uncertain transition.
    toggle_pending = toggle_taken = false; ++toggle_sequence;
}
void request_foreground() {
    std::scoped_lock lock(mutex);
    if (valid() && background_generation == armed_generation && game_front && !toggle_pending) {
        ++toggle_sequence; toggle_pending = true; toggle_taken = false;
    }
}
}
