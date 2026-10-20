// SPDX-License-Identifier: GPL-2.0-only
// SwitchU first-account / delayed Breeze return policy, applied to stock qlaunch.
#include "launch_bridge.hpp"
#include "launch_policy.hpp"
#include "launch_handoff.hpp"
#include "launch_storage.hpp"
#include "breeze_background.hpp"
#include "breeze_channel.hpp"
#include "launch_ecs.hpp"
#include "loader_receipt.h"
#include "home_bridge.hpp"
#include "close_screen.hpp"
#include "remote_nro_input.hpp"
#include "remote_nro_server.hpp"
#include "trace.hpp"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace observer::launch {
namespace {
constexpr Result NotReady = MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
constexpr Result Busy = MAKERESULT(Module_Libnx, LibnxError_AlreadyInitialized);
constexpr Result Cancelled = MAKERESULT(Module_Libnx, LibnxError_BadInput);
struct Root {
    std::weak_ptr<Service> service;
    const void* session{};
    std::uint64_t pid{}, generation{};
    std::uint32_t proxy{};
    bool qlaunch{};
};
std::array<Root, 16> roots;
std::mutex mutex;
std::uint64_t serial{}, qepoch{}, proven{};
Policy policy;
NativeHandoff native_handoff;
std::uint32_t current_application{};
bool enabled{};
ams::os::ThreadType thread;
alignas(0x1000) std::uint8_t stack[0x10000];
std::atomic<bool> signalled{};
std::uint64_t now() { return armTicksToNs(armGetSystemTick()) / 1000000; }
Root* latest_album() {
    Root* out = nullptr;
    for (auto& r : roots) if (!r.qlaunch && !r.service.expired() && (!out || out->generation < r.generation)) out = &r;
    return out;
}
Root* qroot() { for (auto& r : roots) if (r.qlaunch && r.generation == qepoch && !r.service.expired()) return &r; return nullptr; }
struct SavedTls {
    std::array<std::uint8_t, 0x100> bytes;
    SavedTls() { std::memcpy(bytes.data(), ams::sf::hipc::GetMessageBufferOnTls(), bytes.size()); }
    ~SavedTls() { std::memcpy(ams::sf::hipc::GetMessageBufferOnTls(), bytes.data(), bytes.size()); }
};
struct Connection {
    Service connection{}, proxy{}, creator{}, debug{}, functions{}, common{};
    std::uint64_t epoch{};
    ~Connection() { close(); }
    void close() {
        for (auto* s : { &debug, &common, &functions, &creator }) if (serviceIsActive(s)) serviceClose(s);
        if (serviceIsActive(&connection)) serviceClose(&connection);
        proxy = {}; epoch = 0;
    }
    Result open() {
        std::shared_ptr<Service> root;
        std::uint32_t object = 0;
        std::uint64_t generation = 0;
        { std::scoped_lock lock(mutex); auto* q = qroot(); if (!q || !q->proxy) return NotReady;
          root = q->service.lock(); object = q->proxy; generation = q->generation; }
        if (epoch == generation && serviceIsActive(&connection)) return 0;
        close();
        auto rc = serviceClone(root.get(), &connection);
        if (R_SUCCEEDED(rc)) {
            proxy = borrowed(connection, object);
            rc = child(&proxy, 11, &creator);
        }
        if (R_SUCCEEDED(rc)) rc = child(&proxy, 1000, &debug);
        if (R_SUCCEEDED(rc)) rc = child(&proxy, 20, &functions);
        if (R_SUCCEEDED(rc)) rc = child(&proxy, 0, &common);
        if (R_FAILED(rc)) close(); else epoch = generation;
        return rc;
    }
};
Result complete(Service* service, bool& out) {
    u8 value = 1;
    const auto rc = serviceDispatchOut(service, 1, value);
    out = value != 0; return rc;
}
bool background_capable() {
    char value[64]{};
    FILE* file = std::fopen("sdmc:/config/SwitchU/breeze_running", "rb");
    if (!file) return false;
    const auto count = std::fread(value, 1, sizeof(value), file);
    std::fclose(file);
    return background::capable(value, count);
}
Result pop_background(Service& holder, background::Message& message) {
    return background::pop_message(holder, message);
}
bool background_current(const Connection& c, std::uint64_t generation) {
    std::scoped_lock lock(mutex);
    const auto* a = latest_album();
    return qepoch == c.epoch && a && a->generation == generation && proven == generation;
}
Result send_background(Connection& c, Service& holder, std::uint64_t generation,
                       background::Command command, const home::Toggle* toggle = nullptr) {
    if (!background_current(c, generation) || (toggle && !home::matches(*toggle))) return Cancelled;
    const background::Message message{background::Magic, command, 0, 0};
    const auto started = now();
    const auto result = [&](Result value) {
        Record record;
        record.kind = Kind::Library;
        record.object = holder.object_id;
        record.command = command;
        record.argument = static_cast<std::uint32_t>(now() - started);
        record.result = value;
        record.flags = 0x1000;
        push(record);
        return value;
    };
    auto rc = background::send_message(c.creator, holder, message);
    if (R_FAILED(rc)) return result(rc);
    const auto deadline = started + background::AckTimeoutMs;
    while (now() < deadline) {
        if (!background_current(c, generation) || (toggle && !home::matches(*toggle))) return result(Cancelled);
        background::Message reply{};
        if (R_SUCCEEDED(pop_background(holder, reply))) {
            if (background::ack(reply, command)) return result(0);
            if (background::valid(reply) && reply.command == background::RequestForeground) home::request_foreground();
        }
        ams::os::SleepThread(ams::TimeSpan::FromMilliSeconds(10));
    }
    return result(KERNELRESULT(TimedOut)); // One submission only, including lost Ack.
}
void background_tick(Connection& c, Service& holder, std::uint64_t generation) {
    home::Toggle value;
    if (!generation || !serviceIsActive(&holder) || !background_current(c, generation) ||
        !home::background_pair(value) || value.album != generation) return;
    // Drain only our identified holder, bounded even if Breeze floods the pipe.
    for (unsigned i=0; i<8; ++i) {
        background::Message reply{};
        if (R_FAILED(pop_background(holder, reply))) break;
        if (background::valid(reply) && reply.command == background::RequestForeground) home::request_foreground();
    }
    if (!home::take_toggle(value)) return;
    bool library_done = true, game_done = true;
    auto game = borrowed(c.connection, value.application);
    auto rc = complete(&holder, library_done);
    if (R_SUCCEEDED(rc)) rc = complete(&game, game_done);
    if (R_SUCCEEDED(rc) && (library_done || game_done)) rc = NotReady;
    // Normal rendering can need a visible applet window before its UI thread
    // acknowledges the mode change. Give it foreground first; otherwise a
    // depleted hidden buffer queue can deadlock EnterNormal's acknowledgement.
    if (R_SUCCEEDED(rc) && value.game_front) rc = serviceDispatch(&c.functions, 10);
    if (R_SUCCEEDED(rc)) rc = send_background(c, holder, generation,
        value.game_front ? background::EnterNormal : background::EnterHidden, &value);
    if (R_SUCCEEDED(rc) && !home::matches(value)) rc = Cancelled;
    if (R_SUCCEEDED(rc) && !value.game_front) {
            rc = serviceDispatch(&c.functions, 12);
            if (R_SUCCEEDED(rc)) rc = serviceDispatch(&game, 101);
    }
    startup(81, rc, value.game_front ? 4 : 1);
    // A failed hide leaves the native applet in front: restore its normal
    // renderer once, then disarm. Never replay the foreground operation.
    if (R_FAILED(rc) && !value.game_front && background_current(c,generation))
        (void)send_background(c,holder,generation,background::EnterNormal);
    home::finish_toggle(value, R_SUCCEEDED(rc));
}
Result first_account(User& out) {
    auto rc = accountInitialize(AccountServiceType_System);
    if (R_FAILED(rc)) return rc;
    AccountUid users[ACC_USER_LIST_SIZE]{}; s32 count = 0;
    rc = accountListAllUsers(users, ACC_USER_LIST_SIZE, &count);
    if (R_SUCCEEDED(rc) && count > 0) {
        out.words[0] = users[0].uid[0]; out.words[1] = users[0].uid[1];
    }
    accountExit();
    return R_FAILED(rc) ? rc : out.valid() ? 0 : NotReady;
}
Result no_running_game() {
    // Do not rely solely on traced accessors: qlaunch may copy one to an
    // unobserved child session. This is a read-only process query, no attach.
    auto rc = pmdmntInitialize();
    if (R_FAILED(rc)) return rc;
    u64 pid = 0;
    rc = pmdmntGetApplicationProcessId(&pid);
    pmdmntExit();
    if (rc == ams::pm::ResultProcessNotFound().GetValue()) return 0;
    if (R_FAILED(rc)) return rc;
    return pid ? Busy : 0;
}
bool valid_nro(const char* path = NSH_BREEZE_PATH) {
    std::uint8_t header[0x80]{};
    FILE* file = std::fopen(path, "rb");
    if (!file) return false;
    const auto count = std::fread(header, 1, sizeof(header), file);
    const bool ok = count == sizeof(header) && std::memcmp(header + 0x10, "NRO0", 4) == 0;
    std::fclose(file); return ok;
}
bool ticket_file(std::uint64_t ticket, const char* path = NSH_BREEZE_PATH) {
    NshLoaderTicket value{};
    value.magic = 0x5448534e; value.version = 2; value.ticket = ticket;
    std::snprintf(value.path, sizeof(value.path), "%s", path);
    if (!nshLoaderTicketValid(&value)) return false;
    FILE* file = std::fopen(NSH_LOADER_TICKET, "wb");
    if (!file) return false;
    const bool wrote = std::fwrite(&value, sizeof(value), 1, file) == 1;
    const bool closed = std::fclose(file) == 0;
    return wrote && closed && R_SUCCEEDED(fsdevCommitDevice("sdmc"));
}
bool receipt(NshLoaderReceipt& value) {
    FILE* file = std::fopen(NSH_LOADER_RECEIPT, "rb");
    if (!file) return false;
    const bool ok = std::fread(&value, sizeof(value), 1, file) == 1 && std::fgetc(file) == EOF;
    std::fclose(file);
    return ok && value.magic == 0x4c48534e && value.version == 1 && value.path[511] == 0;
}
Result launch_handoff(Connection& c, Service& owned_holder, std::uint64_t owned_generation) {
    const auto snapshot = [&](HandoffSnapshot& out) {
        { std::scoped_lock lock(mutex);
          out.epoch = qepoch;
          const auto* a = latest_album();
          out.album = a ? a->generation : 0;
          out.identified = a && a->generation == proven; }
        u8 active = 1, focus = 0;
        auto rc = serviceDispatchOut(&c.creator, 2, active);
        if (R_SUCCEEDED(rc)) rc = serviceDispatchOut(&c.common, 9, focus);
        out.library_active = active != 0;
        out.qlaunch_in_focus = focus == AppletFocusState_InFocus;
        return rc;
    };
    HandoffSnapshot current{};
    auto rc = snapshot(current);
    if (R_FAILED(rc)) return rc;
    if (current.epoch != c.epoch) return Cancelled;
    Handoff handoff(current, now());
    for (;;) {
        switch (handoff.next(current, now())) {
            case HandoffAction::Ready: startup(77); return 0;
            case HandoffAction::Busy: return Busy;
            case HandoffAction::Cancelled: return Cancelled;
            case HandoffAction::TimedOut: return KERNELRESULT(TimedOut);
            case HandoffAction::Home: {
                // Our own retained holder is not in qlaunch's UI bookkeeping.
                // Ask it to exit normally; native qlaunch handles other holders.
                // Never terminate a holder or close an unknown NRO here.
                (void)home::command("home off");
                if (owned_generation == current.album && serviceIsActive(&owned_holder)) {
                    bool done = false;
                    rc = complete(&owned_holder, done);
                    if (R_SUCCEEDED(rc) && !done) rc = serviceDispatch(&owned_holder, 20);
                    startup(79, rc);
                    if (R_FAILED(rc)) return rc;
                }
                const u32 button = AppletSystemButtonType_HomeButtonShortPressing;
                rc = serviceDispatchIn(&c.debug, 10, button);
                startup(76, rc);
                if (R_FAILED(rc)) return rc;
                break;
            }
            case HandoffAction::Wait: break;
        }
        ams::os::SleepThread(ams::TimeSpan::FromMilliSeconds(20));
        rc = snapshot(current);
        if (R_FAILED(rc)) return rc;
    }
}
Result open_breeze(Connection& c, Service& holder, std::uint64_t ticket,
                   const char* path = NSH_BREEZE_PATH, bool restoring = true) {
    if (!valid_nro(path)) return NotReady;
    // Match the upstream foreground applet runner. An AllForeground child
    // must not be started while its qlaunch parent is still behind the game.
    auto rc = serviceDispatch(&c.functions, 10);
    u8 focus = 0;
    const auto deadline = now() + 2000;
    while (R_SUCCEEDED(rc) && now() < deadline) {
        rc = serviceDispatchOut(&c.common, 9, focus);
        if (R_FAILED(rc) || focus == AppletFocusState_InFocus) break;
        ams::os::SleepThread(ams::TimeSpan::FromMilliSeconds(20));
    }
    if (R_SUCCEEDED(rc) && focus != AppletFocusState_InFocus) rc = KERNELRESULT(TimedOut);
    startup(69, rc, focus);
    if (R_FAILED(rc) || !ticket_file(ticket, path)) return R_FAILED(rc) ? rc : NotReady;
    rc = register_loader();
    if (R_FAILED(rc)) return rc;
    // Breeze needs a normal foreground initialization on the tested firmware.
    // Resume the game only after its authenticated 6801 state is available.
    const struct { u32 id, mode; } input{AppletId_LibraryAppletPhotoViewer, LibAppletMode_AllForeground};
    rc = serviceDispatchIn(&c.creator, 0, input, .out_num_objects = 1, .out_objects = &holder);
    startup(70, rc);
    LibAppletArgs args{};
    args.CommonArgs_version = 1; args.CommonArgs_size = sizeof(args);
    args.LaVersion = 0x10000; args.tick = armGetSystemTick();
    if (R_SUCCEEDED(rc)) rc = push_storage(&c.creator, &holder, 100, &args, sizeof(args));
    startup(71, rc);
    const u8 album = AlbumLaArg_ShowAllAlbumFilesForHomeMenu;
    if (R_SUCCEEDED(rc)) rc = push_storage(&c.creator, &holder, 100, &album, sizeof(album));
    startup(72, rc);
    if (R_SUCCEEDED(rc)) {
        std::scoped_lock lock(mutex);
        if (qepoch != c.epoch || (restoring && (policy.phase != Phase::Restoring || policy.epoch != c.epoch))) rc = Cancelled;
    }
    if (R_SUCCEEDED(rc)) {
        // Publish before Start: the NRO may initialize AM before it returns.
        home::adopt_library(holder.object_id, armGetSystemTick());
        rc = serviceDispatch(&holder, 10);
        startup(73, rc);
    }
    if (R_FAILED(rc)) {
        if (serviceIsActive(&holder)) serviceClose(&holder);
        (void)unregister_loader();
    }
    return rc;
}
bool connection_current(const Connection& c) {
    std::scoped_lock lock(mutex);
    return qepoch == c.epoch;
}
// An explicit PC NRO launch replaces the Album slot internally. No separate
// client Stop-NRO call is required. Other library applets are never terminated.
Result replace_album(Connection& c, Service& holder, std::uint64_t& holder_generation) {
    if (serviceIsActive(&holder)) {
        bool done = false;
        auto rc = complete(&holder, done);
        if (R_FAILED(rc)) return rc;
        if (done) { serviceClose(&holder); holder_generation = 0; rc = unregister_loader(); }
        if (R_FAILED(rc)) return rc;
    }
    u8 occupied = 0;
    auto rc = serviceDispatchOut(&c.creator, 2, occupied);
    startup(92, rc, occupied);
    if (R_FAILED(rc)) return rc;
    // AreAnyLibraryAppletsLeft may exclude the holder created through our
    // cloned session. Its live accessor is still the owned Album slot.
    bool album_live = false;
    { std::scoped_lock lock(mutex); album_live = latest_album() != nullptr; }
    if (!occupied && !serviceIsActive(&holder) && !album_live) return 0;
    home::LibrarySnapshot snapshot;
    if (!home::library_snapshot(snapshot)) { startup(93, Busy); return Busy; }
    if (serviceIsActive(&holder) && holder.object_id != snapshot.object) { startup(94, Busy, snapshot.object); return Busy; }
    auto previous = borrowed(c.connection, snapshot.object);
    LibAppletInfo info{};
    rc = serviceDispatchOut(&previous, 120, info);
    startup(95, rc, static_cast<std::uint32_t>(info.appletId));
    if (R_FAILED(rc)) return rc;
    if (info.appletId != AppletId_LibraryAppletPhotoViewer) return Busy;
    (void)home::command("home off");
    if (!connection_current(c) || !home::matches_library(snapshot)) return Cancelled;
    rc = serviceDispatch(&c.functions, 10);
    if (R_FAILED(rc)) return rc;
    if (holder_generation && serviceIsActive(&holder) && background_current(c, holder_generation)) {
        rc = send_background(c, holder, holder_generation, background::Release);
        // An explicit exit must also work when Breeze's UI cannot acknowledge.
        // The same verified Album accessor still gets native exit-or-terminate.
        if (R_FAILED(rc)) startup(98, rc);
    }
    if (!connection_current(c) || !home::matches_library(snapshot)) return Cancelled;
    rc = serviceDispatch(&previous, 20); // RequestExit once.
    if (R_FAILED(rc)) return rc;
    bool done = false;
    const auto wait_complete = [&](std::uint64_t timeout) -> Result {
        const auto until = now() + timeout;
        do {
            if (!connection_current(c)) return Cancelled;
            if (!home::matches_library(snapshot)) {
                u8 left = 1;
                const auto checked = serviceDispatchOut(&c.creator, 2, left);
                if (R_SUCCEEDED(checked) && !left) { done = true; return 0; }
                return Cancelled;
            }
            auto result = complete(&previous, done);
            if (R_FAILED(result)) {
                u8 left = 1;
                const auto checked = serviceDispatchOut(&c.creator, 2, left);
                if (R_SUCCEEDED(checked) && !left) { done = true; return 0; }
                return result;
            }
            if (done) return 0;
            ams::os::SleepThread(ams::TimeSpan::FromMilliSeconds(20));
        } while (now() < until);
        return KERNELRESULT(TimedOut);
    };
    rc = wait_complete(1500);
    if (rc == KERNELRESULT(TimedOut)) {
        // Match libnx RequestExitOrTerminate, but only for the same verified
        // PhotoViewer process/accessor. hbloader may have returned to hbmenu.
        if (!connection_current(c) || !home::matches_library(snapshot)) return Cancelled;
        rc = serviceDispatch(&previous, 25);
        if (R_SUCCEEDED(rc)) rc = wait_complete(2000);
    }
    if (R_FAILED(rc) || !done) return R_FAILED(rc) ? rc : NotReady;
    if (serviceIsActive(&holder)) serviceClose(&holder);
    holder_generation = 0;
    rc = unregister_loader();
    if (R_FAILED(rc)) return rc;
    // Native completion may precede the relay observing the closed Album
    // sessions. Do not mistake that short gap for a foreign active NRO.
    const auto closed = now() + 2000;
    while (now() < closed) {
        if (!connection_current(c)) return Cancelled;
        { std::scoped_lock lock(mutex); const auto* a = latest_album();
          if (!a) return 0;
          if (a->pid != snapshot.pid) return Cancelled; }
        ams::os::SleepThread(ams::TimeSpan::FromMilliSeconds(20));
    }
    return KERNELRESULT(TimedOut);
}
__attribute__((noinline)) Result launch_nro(Connection& c, Service& holder, std::uint64_t& holder_generation,
                  const nshome::remote::Request& request) {
    if (!valid_nro(request.path.c_str())) return NotReady;
    auto rc = replace_album(c, holder, holder_generation);
    if (R_FAILED(rc)) return rc;
    { std::scoped_lock lock(mutex); policy = {}; }
    (void)home::command("home off");
    const auto ticket = armGetSystemTick();
    rc = open_breeze(c, holder, ticket, request.path.c_str(), false);
    startup(90, rc, static_cast<std::uint32_t>(request.id));
    if (R_FAILED(rc)) return rc;
    const auto deadline = now() + 15000;
    while (now() < deadline) {
        if (!connection_current(c)) return Cancelled;
        NshLoaderReceipt value{};
        if (receipt(value) && value.ticket == ticket && value.sequence == 1) {
            if (value.result) return value.result;
            if (value.ready && std::strcmp(value.path, request.path.c_str()) == 0) {
                // Other NROs need no Breeze server. A short-lived NRO may
                // already have returned; the receipt still proves its mapping.
                if (request.path != NSH_BREEZE_PATH) return 0;
                std::uint64_t generation = 0;
                { std::scoped_lock lock(mutex); const auto* a = latest_album();
                  if (a && a->pid == value.pid && a->generation == proven) generation = a->generation; }
                if (generation) {
                    // Explicitly opening Breeze leaves it foreground. Arm the
                    // optional game toggle only when a live game pair exists.
                    holder_generation = generation;
                    (void)home::identify_breeze(generation, false, background_capable());
                    return 0;
                }
            }
        }
        ams::os::SleepThread(ams::TimeSpan::FromMilliSeconds(20));
    }
    return KERNELRESULT(TimedOut);
}
// Native Album accessors belong to qlaunch: borrow, never close their objects.
bool manual_breeze(Connection& c, home::LibrarySnapshot& snapshot, std::uint64_t& generation) {
    std::shared_ptr<Service> root;
    std::uint32_t proxy = 0;
    {
        std::scoped_lock lock(mutex);
        const auto* a = latest_album();
        if (!a || !a->proxy || a->generation != proven || policy.pending()) return false;
        root = a->service.lock(); proxy = a->proxy; generation = a->generation;
    }
    if (!root || !home::library_snapshot(snapshot) || snapshot.qlaunch != c.epoch) return false;
    Service cloned{}, common{};
    auto rc = serviceClone(root.get(), &cloned);
    if (R_SUCCEEDED(rc)) {
        auto applet = borrowed(cloned, proxy);
        rc = child(&applet, 0, &common);
    }
    u8 focus = 0;
    if (R_SUCCEEDED(rc)) rc = serviceDispatchOut(&common, 9, focus);
    if (serviceIsActive(&common)) serviceClose(&common);
    if (serviceIsActive(&cloned)) serviceClose(&cloned);
    if (R_FAILED(rc) || focus != AppletFocusState_InFocus || !background_current(c,generation)) return false;
    return home::matches_library(snapshot) && home::identify_breeze(generation,false,background_capable());
}
// Native qlaunch owns the close intent and its result presentation. Direct
// RequestExit/Terminate is insufficient: even Result=0 can show its error UI.
struct CloseContext { Connection* connection{}; u64 pid{}, title{}; u32 object{}; bool confirm{}; };
CloseContext* closing{}; // Worker-owned; input callback runs synchronously.
Result application_pid(u64& pid) {
    auto rc=pmdmntInitialize();
    if (R_FAILED(rc)) return rc;
    rc=pmdmntGetApplicationProcessId(&pid); pmdmntExit(); return rc;
}
bool close_identity(const CloseContext& value) {
    auto& c=*value.connection;
    std::scoped_lock lock(mutex);
    if (qepoch!=c.epoch || current_application!=value.object) return false;
    u64 pid=0,title=0;
    if (R_FAILED(application_pid(pid)) || pid!=value.pid) return false;
    auto app=borrowed(c.connection,value.object);
    return R_SUCCEEDED(serviceDispatchOut(&app,120,title)) && title==value.title;
}
bool close_input_allowed() {
    if (!closing || !close_identity(*closing)) return false;
    u8 focus=0;
    if (R_FAILED(serviceDispatchOut(&closing->connection->common,9,focus)) || focus!=AppletFocusState_InFocus) return false;
    // Rechecked after virtual-controller discovery, immediately before input.
    return stock_close_screen(closing->confirm) && close_identity(*closing);
}
Result stop_game(Connection& c, Service& holder, std::uint64_t& holder_generation,
                 const nshome::remote::Request& request) {
    CloseContext context{&c,0,request.titleId,0,false};
    {
        std::scoped_lock lock(mutex);
        if (qepoch!=c.epoch || !current_application || policy.pending()) return NotReady;
        context.object=current_application;
        // Do not replace DBI/other NROs to stop a game. Only a proven Breeze.
        const auto* a=latest_album();
        if (a && a->generation!=proven) return Busy;
    }
    auto rc=application_pid(context.pid);
    if (R_FAILED(rc) || !context.pid) return R_FAILED(rc) ? rc : NotReady;
    if (!close_identity(context)) return Cancelled;
    // Release this console's single Album slot before giving qlaunch input.
    rc=replace_album(c,holder,holder_generation);
    if (R_FAILED(rc)) return rc;
    (void)home::command("home off");
    { std::scoped_lock lock(mutex); policy={}; }
    rc=serviceDispatch(&c.functions,10);
    if (R_FAILED(rc)) return rc;
    closing=&context;
    struct Reset { ~Reset() { closing=nullptr; } } reset;
    const auto wait_screen=[&](u64 timeout) {
        const auto until=now()+timeout;
        do {
            if (!close_identity(context)) return false;
            if (close_input_allowed()) return true;
            ams::os::SleepThread(ams::TimeSpan::FromMilliSeconds(100));
        } while (now()<until);
        return false;
    };
    if (!close_input_allowed()) {
        // Foreground ownership alone does not enter qlaunch's HOME scene.
        // Deliver one stock HOME message after disarming the old pair.
        const u32 button=AppletSystemButtonType_HomeButtonShortPressing;
        rc=serviceDispatchIn(&c.debug,10,button);
        startup(113,rc);
        if (R_FAILED(rc)) return rc;
    }
    if (!wait_screen(3000)) return NotReady;
    bool attempted=false;
    rc=nshome::remote_input::pressButtonIf(HidNpadButton_X,100,close_input_allowed,&attempted);
    startup(110,rc,attempted);
    if (R_FAILED(rc) || !attempted) return R_FAILED(rc) ? rc : Cancelled;
    context.confirm=true;
    if (!wait_screen(3000)) return NotReady;
    attempted=false;
    rc=nshome::remote_input::pressButtonIf(HidNpadButton_A,100,close_input_allowed,&attempted);
    startup(111,rc,attempted);
    if (R_FAILED(rc) || !attempted) return R_FAILED(rc) ? rc : Cancelled;
    const auto deadline=now()+5000;
    bool exited=false;
    do {
        if (!connection_current(c)) return Cancelled;
        u64 pid=0; rc=application_pid(pid);
        if (rc==ams::pm::ResultProcessNotFound().GetValue() || (R_SUCCEEDED(rc) && !pid)) { exited=true; break; }
        if (R_FAILED(rc)) return rc;
        if (pid!=context.pid) return Cancelled;
        ams::os::SleepThread(ams::TimeSpan::FromMilliSeconds(20));
    } while (now()<deadline);
    if (!exited) return KERNELRESULT(TimedOut);
    closing=nullptr;
    // A human may have opened a new game/NRO while the old game closed.
    if (no_running_game()!=0) return Cancelled;
    { std::scoped_lock lock(mutex); if (latest_album()) return Cancelled; }
    // Restore Breeze for the PC connection, after the native close has ended.
    nshome::remote::Request restore;
    restore.path=NSH_BREEZE_PATH;
    return launch_nro(c,holder,holder_generation,restore);
}
void worker(void*) {
    Connection connection;
    Service holder{};
    std::uint64_t ticket = 0, holder_generation = 0;
    home::LibrarySnapshot manual_snapshot{};
    std::uint64_t manual_generation = 0, manual_check = 0;
    for (;;) {
        auto time = now();
        const auto connected = connection.open();
        if (R_SUCCEEDED(connected)) background_tick(connection, holder, holder_generation);
        if (manual_generation && (!home::matches_library(manual_snapshot) ||
            !background_current(connection,manual_generation))) manual_generation = 0;
        if (R_SUCCEEDED(connected) && !serviceIsActive(&holder)) {
            if (!manual_generation && time >= manual_check) {
                manual_check = time + 250;
                std::uint64_t generation = 0;
                if (manual_breeze(connection,manual_snapshot,generation)) {
                    manual_generation = generation;
                    startup(100,0,static_cast<std::uint32_t>(generation));
                }
            }
            if (manual_generation) {
                auto native = borrowed(connection.connection,manual_snapshot.object);
                background_tick(connection,native,manual_generation);
            }
        }
        NativeIntent native{};
        bool handoff = false;
        {
            std::scoped_lock lock(mutex);
            const auto* a = latest_album();
            handoff = native_handoff.take(connection.epoch, a ? a->pid : 0, policy.pending(), native);
        }
        if (handoff && R_SUCCEEDED(connected) && serviceIsActive(&holder)) {
            home::LibrarySnapshot snapshot{};
            if (home::library_snapshot(snapshot) && snapshot.qlaunch == native.epoch &&
                snapshot.pid == native.pid && snapshot.object == holder.object_id) {
                { std::scoped_lock lock(mutex); policy = {}; }
                const auto rc = replace_album(connection, holder, holder_generation);
                startup(97, rc, native.reason == NativeReason::Launch ? 50 : 20);
                if (R_SUCCEEDED(rc) && native.reason == NativeReason::Home && connection_current(connection)) {
                    const u32 button = AppletSystemButtonType_HomeButtonShortPressing;
                    startup(99, serviceDispatchIn(&connection.debug, 10, button));
                }
                // Native launch message 50 is already queued in qlaunch.
                // Never issue another game launch or select a user here.
            }
        }
        nshome::remote::Request request;
        if (nshome::remote::take(request)) {
            if (request.kind == nshome::remote::RequestKind::StopGame) {
                const auto rc = R_SUCCEEDED(connected) ? stop_game(connection, holder, holder_generation, request) : connected;
                startup(112, rc, static_cast<std::uint32_t>(request.id));
                nshome::remote::finish(request.id, R_SUCCEEDED(rc) ? "finished" : "failed", rc);
                continue;
            }
            if (request.kind == nshome::remote::RequestKind::Nro) {
                const auto rc = R_SUCCEEDED(connected) ? launch_nro(connection, holder, holder_generation, request) : connected;
                startup(91, rc, static_cast<std::uint32_t>(request.id));
                nshome::remote::finish(request.id, R_SUCCEEDED(rc) ? "started" : "failed", rc);
                continue;
            }
            Result rc = request.kind == nshome::remote::RequestKind::StartGame ? connected : NotReady;
            User selected{};
            if (R_SUCCEEDED(rc)) rc = no_running_game();
            if (R_SUCCEEDED(rc)) rc = first_account(selected);
            if (R_SUCCEEDED(rc)) {
                rc = replace_album(connection, holder, holder_generation);
            }
            if (R_SUCCEEDED(rc)) {
                rc = launch_handoff(connection, holder, holder_generation);
                if (R_FAILED(rc)) startup(78, rc);
            }
            // HOME can release an older native pending launch. Never submit a
            // second launch if a game appeared while qlaunch reclaimed focus.
            if (R_SUCCEEDED(rc)) rc = no_running_game();
            time = now();
            if (R_SUCCEEDED(rc)) {
                std::scoped_lock lock(mutex);
                if (current_application) {
                    auto game = borrowed(connection.connection, current_application); bool done = false;
                    rc = complete(&game, done);
                    if (R_SUCCEEDED(rc) && !done) rc = Busy;
                }
                if (R_SUCCEEDED(rc) && !policy.begin(Origin::Remote6802, request.id, request.titleId, connection.epoch, selected, time)) rc = NotReady;
            }
            if (R_SUCCEEDED(rc)) {
                std::scoped_lock lock(mutex);
                const auto user = policy.user;
                const struct { u8 flag; u64 title; } in{0, request.titleId};
                // Native user-aware launch keeps all application ownership in qlaunch.
                if (!policy.submit(request.id, connection.epoch) || qepoch != connection.epoch) rc = Cancelled;
                else rc = serviceDispatchIn(&connection.debug, 30, in,
                    .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_In, SfBufferAttr_HipcMapAlias | SfBufferAttr_In },
                    .buffers = { { &user, sizeof(user) }, { nullptr, 0 } });
                startup(61, rc, 6802);
            }
            if (R_FAILED(rc)) {
                { std::scoped_lock lock(mutex); if (policy.id == request.id) policy.fail(rc); }
                nshome::remote::finish(request.id, "failed", rc);
            }
        }
        Policy state;
        { std::scoped_lock lock(mutex); policy.check(qepoch, time); state = policy; }
        if (state.pending() && serviceIsActive(&holder)) {
            bool done = false;
            const auto rc = complete(&holder, done);
            if (R_SUCCEEDED(rc) && done) {
                serviceClose(&holder); holder_generation = 0; (void)unregister_loader();
                std::scoped_lock lock(mutex); policy.retry_restore(time);
            }
        }
        // Receipt proves the actual mapped NRO, ticket and process; Album alone is insufficient.
        if (state.phase == Phase::Restoring && serviceIsActive(&holder)) {
            NshLoaderReceipt value{};
            if (receipt(value) && value.ticket == ticket && value.ready && !value.result &&
                std::strcmp(value.path, NSH_BREEZE_PATH) == 0) {
                std::uint64_t generation = 0;
                { std::scoped_lock lock(mutex); auto* a = latest_album();
                  if (a && a->pid == value.pid && a->generation == proven) generation = a->generation; }
                if (generation) {
                    auto game = borrowed(connection.connection, state.application);
                    const bool background = background_capable();
                    auto rc = background ? send_background(connection, holder, generation, background::EnterHidden) : 0;
                    startup(80, rc, background);
                    { std::scoped_lock lock(mutex);
                      if (policy.phase != Phase::Restoring || policy.epoch != connection.epoch) rc = Cancelled; }
                    if (R_SUCCEEDED(rc)) rc = serviceDispatch(&connection.functions, 12);
                    if (R_SUCCEEDED(rc)) rc = serviceDispatch(&game, 101);
                    startup(74, rc);
                    if (R_SUCCEEDED(rc) && home::identify_breeze(generation, true, background)) {
                        holder_generation = generation;
                        std::scoped_lock lock(mutex); policy.ready(connection.epoch, generation, true);
                        startup(65, 0, static_cast<std::uint32_t>(generation));
                    } else if (R_FAILED(rc)) {
                        if (background && background_current(connection,generation))
                            (void)send_background(connection,holder,generation,background::EnterNormal);
                        std::scoped_lock lock(mutex); policy.fail(rc);
                    }
                }
            }
        }
        if (state.due(time) && R_SUCCEEDED(connected) && !serviceIsActive(&holder)) {
            bool occupied = false;
            std::uint64_t existing_breeze = 0;
            { std::scoped_lock lock(mutex); const auto* a = latest_album(); occupied = a != nullptr;
              if (a && a->generation == proven) existing_breeze = a->generation; }
            auto game = borrowed(connection.connection, state.application);
            bool done = true; u8 applets = 1, global_applets = 1;
            auto rc = complete(&game, done);
            if (R_SUCCEEDED(rc) && !done) rc = serviceDispatchOut(&game, 111, applets);
            if (R_SUCCEEDED(rc) && !done && existing_breeze && home::identify_breeze(existing_breeze, true)) {
                std::scoped_lock lock(mutex); policy.ready(connection.epoch, existing_breeze, true);
            } else {
              if (R_SUCCEEDED(rc)) rc = serviceDispatchOut(&connection.creator, 2, global_applets);
              if (R_SUCCEEDED(rc) && !done && !applets && !global_applets && !occupied) {
                ticket = armGetSystemTick();
                { std::scoped_lock lock(mutex);
                  if (!policy.due(time) || policy.epoch != connection.epoch) continue;
                  policy.restoring(); }
                rc = open_breeze(connection, holder, ticket);
                startup(64, rc);
                std::scoped_lock lock(mutex);
                if (R_FAILED(rc)) { holder_generation = 0; policy.retry_restore(time); }
              } else {
                std::scoped_lock lock(mutex);
                if (done) policy.cancel(); else policy.retry(time);
              }
            }
        }
        { std::scoped_lock lock(mutex); state = policy; }
        if (state.id && (state.phase == Phase::Ready || state.phase == Phase::Failed || state.phase == Phase::Cancelled))
            nshome::remote::finish(state.id, state.phase == Phase::Ready ? "started" : "failed",
                                   state.phase == Phase::Ready ? 0 : state.result ? state.result : Cancelled);
        // Finished holders are released without terminating an active replacement NRO.
        if (serviceIsActive(&holder) && !state.pending()) {
            bool done = false;
            if (R_SUCCEEDED(complete(&holder, done)) && done) {
                serviceClose(&holder); holder_generation = 0; (void)unregister_loader();
                // qlaunch does not own the cloned creator's UI bookkeeping.
                // A generic tool's ExitProcessAndReturn therefore also needs
                // its real parent brought forward after completion.
                if (R_SUCCEEDED(connected) && connection_current(connection)) {
                    const u32 button = AppletSystemButtonType_HomeButtonShortPressing;
                    const auto rc = serviceDispatchIn(&connection.debug, 10, button);
                    startup(96, rc);
                }
            }
        }
        if (!signalled.exchange(false)) ams::os::SleepThread(ams::TimeSpan::FromMilliSeconds(100));
    }
}
}
void request_stock_home(const void* session) {
    if (!home::is_qlaunch(session)) return;
    std::scoped_lock lock(mutex);
    const auto* a = latest_album();
    if (a && !policy.pending()) native_handoff.request(qepoch, a->pid, NativeReason::Home);
}
void wake() { signalled.store(true); }
bool start() {
    const auto rc = ams::os::CreateThread(&thread, worker, nullptr, stack, sizeof(stack), 17, 3);
    startup(60, rc.GetValue());
    if (rc.IsSuccess()) {
        { std::scoped_lock lock(mutex); enabled = true; }
        ams::os::SetThreadNamePointer(&thread, "nshome-game-start"); ams::os::StartThread(&thread);
    }
    return rc.IsSuccess();
}
void register_root(const std::shared_ptr<Service>& forward, std::uint64_t pid, bool qlaunch) {
    std::scoped_lock lock(mutex);
    for (auto& r : roots) if (r.service.expired()) {
        r = {forward, nullptr, pid, ++serial, 0, qlaunch};
        if (qlaunch) { qepoch = r.generation; current_application = 0; policy.cancel(); }
        return;
    }
}
std::uint64_t bind_root(const void* session, std::uint64_t pid) {
    std::scoped_lock lock(mutex);
    Root* candidate = nullptr;
    for (auto& r : roots) if (r.pid == pid && !r.service.expired()) {
        if (r.session == session) return r.generation;
        if (!r.session && (!candidate || r.generation > candidate->generation)) candidate = &r;
    }
    if (candidate) { candidate->session = session; return candidate->generation; }
    return 0;
}
void before_forward(const void* session, Kind kind, const Request& request) {
    if (request.closing || !request.valid) return;
    std::scoped_lock lock(mutex);
    if (!enabled) return;
    Root* source = nullptr;
    for (auto& r : roots) if (r.session == session && !r.service.expired()) source = &r;
    if (!source) return;
    if (kind == Kind::LibrarySelf && request.command == 90 && !source->qlaunch) {
        // A 6801/manual applet launch supersedes the PC transaction. Its native
        // profile selection and lifecycle remain entirely stock.
        policy.cancel();
    }
}
void observe(const void* session, Kind kind, const Request& request, const Reply& reply) {
    std::scoped_lock lock(mutex);
    Root* source = nullptr;
    for (auto& r : roots) if (r.session == session && !r.service.expired()) source = &r;
    if (!source) return;
    if (request.closing) {
        if (source->qlaunch && kind == Kind::Application && request.object == current_application) {
            current_application = 0;
            // A clean accessor may replace this one before application Start.
            if (policy.phase != Phase::Requested) policy.cancel();
        }
        return;
    }
    if (!reply.valid || reply.result) {
        if (source->qlaunch && kind == Kind::Application && request.object == policy.application && request.command == 10)
            policy.fail(reply.valid ? reply.result : NotReady);
        return;
    }
    if (kind == Kind::Root && reply.object) source->proxy = reply.object;
    if (!source->qlaunch) return;
    if (kind == Kind::Common && request.command == 1 && (reply.scalar == 20 || reply.scalar == 25)) policy.cancel();
    // Message 50 remains visible to stock qlaunch, which owns the launch and selector logic.
    if (kind == Kind::Common && request.command == 1 && reply.scalar == 50) {
        startup(67, 0, policy.pending());
        const auto* a = latest_album();
        if (a && !policy.pending()) native_handoff.request(qepoch, a->pid, NativeReason::Launch);
    }
    if (kind == Kind::AppCreator && reply.object) {
        current_application = reply.object;
        if (policy.phase == Phase::Requested) {
            SavedTls tls;
            auto root = source->service.lock(); auto app = borrowed(*root, reply.object);
            u64 title = 0;
            const auto rc = serviceDispatchOut(&app, 120, title);
            if (R_SUCCEEDED(rc)) {
                if (!policy.bind(title, qepoch, reply.object)) policy.cancel();
            } else policy.fail(rc);
        }
    }
    if (kind == Kind::Application && policy.phase == Phase::Requested && request.object == policy.application && request.command == 10) {
        policy.started(request.object, now()); startup(63);
    }
    if (kind == Kind::Application && policy.phase == Phase::Requested && request.object == policy.application && request.command == 121)
        startup(62, 0, request.argument); // qlaunch owns the launch parameter.
    if (kind == Kind::Application && request.object == policy.application &&
        (request.command == 20 || request.command == 25 || request.command == 30 || (request.command == 1 && reply.scalar))) policy.cancel();
}

// Identity probes share the BSD lifetime, but never wait on the accept worker.
namespace {
struct IdentityProbe {
    enum Phase { Connect, Greeting, AuthSend, AuthReply, StateSend, StateReply } phase{};
    int fd = -1;
    std::uint64_t generation{}, deadline{}, next{};
    std::size_t sent{};
    std::string line, output;
    void close() { if (fd >= 0) ::close(fd); fd = -1; line.clear(); output.clear(); sent = 0; }
    void writing(Phase value, std::string text) { phase = value; output = std::move(text); sent = 0; line.clear(); }
} probe;
}
void network_reset() { probe.close(); }
void network_tick(const char* pairing_code) {
    const auto time = now();
    std::uint64_t generation = 0;
    { std::scoped_lock lock(mutex); const auto* a = latest_album(); if (a && a->generation != proven) generation = a->generation; }
    if (probe.fd >= 0 && (generation != probe.generation || time >= probe.deadline)) probe.close();
    if (!generation) return;
    if (probe.fd < 0) {
        if (time < probe.next) return;
        probe.next = time + 1000;
        probe.fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (probe.fd < 0) return;
        const auto flags = ::fcntl(probe.fd,F_GETFL,0);
        if (flags < 0 || ::fcntl(probe.fd,F_SETFL,flags | O_NONBLOCK) < 0) { probe.close(); return; }
        probe.generation = generation; probe.deadline = time + 1500;
        sockaddr_in address{}; address.sin_family = AF_INET; address.sin_port = htons(6801); address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        const auto rc = ::connect(probe.fd,reinterpret_cast<sockaddr*>(&address),sizeof(address));
        if (rc < 0 && errno != EINPROGRESS) { probe.close(); return; }
        probe.phase = rc == 0 ? IdentityProbe::Greeting : IdentityProbe::Connect;
    }
    if (probe.phase == IdentityProbe::Connect) {
        pollfd pending{probe.fd,POLLOUT,0};
        if (::poll(&pending,1,0) <= 0) return;
        int error = -1; socklen_t length = sizeof(error);
        if (::getsockopt(probe.fd,SOL_SOCKET,SO_ERROR,&error,&length) < 0 || error) { probe.close(); return; }
        probe.phase = IdentityProbe::Greeting;
    }
    if (probe.phase == IdentityProbe::AuthSend || probe.phase == IdentityProbe::StateSend) {
        const auto count = ::send(probe.fd,probe.output.data()+probe.sent,probe.output.size()-probe.sent,0);
        if (count > 0) probe.sent += count;
        else if (count == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) { probe.close(); return; }
        if (probe.sent != probe.output.size()) return;
        probe.phase = probe.phase == IdentityProbe::AuthSend ? IdentityProbe::AuthReply : IdentityProbe::StateReply;
    }
    char bytes[512];
    const auto count = ::recv(probe.fd,bytes,sizeof(bytes),0);
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) return;
    if (count <= 0) { probe.close(); return; }
    for (ssize_t i=0; i<count; ++i) {
        if (bytes[i] != '\n') {
            if (probe.line.size() >= 8191) { probe.close(); return; }
            probe.line.push_back(bytes[i]); continue;
        }
        if (probe.phase == IdentityProbe::Greeting) {
            probe.writing(IdentityProbe::AuthSend,std::string("auth ")+pairing_code+"\n"); return;
        }
        if (probe.phase == IdentityProbe::AuthReply) {
            if (probe.line.compare(0,3,"+OK") != 0) { probe.close(); return; }
            probe.writing(IdentityProbe::StateSend,"state 1\n"); return;
        }
        const bool ok = is_breeze_state(probe.line.c_str());
        probe.close();
        if (ok) {
            std::scoped_lock lock(mutex); const auto* a = latest_album();
            if (a && a->generation == generation) proven = generation;
        }
        return;
    }
}
}
