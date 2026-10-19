// SPDX-License-Identifier: GPL-2.0-only
// SwitchU first-account / delayed Breeze return policy, applied to stock qlaunch.
#include "launch_bridge.hpp"
#include "launch_policy.hpp"
#include "launch_storage.hpp"
#include "launch_ecs.hpp"
#include "loader_receipt.h"
#include "home_bridge.hpp"
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
    Service connection{}, proxy{}, creator{}, debug{}, functions{};
    std::uint64_t epoch{};
    ~Connection() { close(); }
    void close() {
        for (auto* s : { &debug, &functions, &creator }) if (serviceIsActive(s)) serviceClose(s);
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
        if (R_FAILED(rc)) close(); else epoch = generation;
        return rc;
    }
};
Result complete(Service* service, bool& out) {
    u8 value = 1;
    const auto rc = serviceDispatchOut(service, 1, value);
    out = value != 0; return rc;
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
bool valid_nro() {
    std::uint8_t header[0x80]{};
    FILE* file = std::fopen(NSH_BREEZE_PATH, "rb");
    if (!file) return false;
    const auto count = std::fread(header, 1, sizeof(header), file);
    const bool ok = count == sizeof(header) && std::memcmp(header + 0x10, "NRO0", 4) == 0;
    std::fclose(file); return ok;
}
bool ticket_file(std::uint64_t ticket) {
    FILE* file = std::fopen(NSH_LOADER_TICKET, "wb");
    if (!file) return false;
    const bool wrote = std::fwrite(&ticket, sizeof(ticket), 1, file) == 1;
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
Result open_breeze(Connection& c, Service& holder, std::uint64_t ticket) {
    if (!valid_nro() || !ticket_file(ticket)) return NotReady;
    auto rc = register_loader();
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
        if (policy.phase != Phase::Restoring || policy.epoch != c.epoch || qepoch != c.epoch) rc = Cancelled;
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
void worker(void*) {
    Connection connection;
    Service holder{};
    std::uint64_t ticket = 0;
    for (;;) {
        const auto time = now();
        const auto connected = connection.open();
        nshome::remote::Request request;
        if (nshome::remote::take(request)) {
            Result rc = request.kind == nshome::remote::RequestKind::StartGame ? connected : NotReady;
            User selected{};
            if (R_SUCCEEDED(rc)) rc = no_running_game();
            if (R_SUCCEEDED(rc)) rc = first_account(selected);
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
                serviceClose(&holder); (void)unregister_loader();
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
                    auto rc = serviceDispatch(&connection.functions, 12);
                    if (R_SUCCEEDED(rc)) rc = serviceDispatch(&game, 101);
                    startup(74, rc);
                    if (R_SUCCEEDED(rc) && home::identify_breeze(generation, true)) {
                        std::scoped_lock lock(mutex); policy.ready(connection.epoch, generation, true);
                        startup(65, 0, static_cast<std::uint32_t>(generation));
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
                if (R_FAILED(rc)) policy.retry_restore(time);
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
            if (R_SUCCEEDED(complete(&holder, done)) && done) { serviceClose(&holder); (void)unregister_loader(); }
        }
        if (!signalled.exchange(false)) ams::os::SleepThread(ams::TimeSpan::FromMilliSeconds(100));
    }
}
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
void bind_root(const void* session, std::uint64_t pid) {
    std::scoped_lock lock(mutex);
    Root* candidate = nullptr;
    for (auto& r : roots) if (r.pid == pid && !r.service.expired()) {
        if (r.session == session) return;
        if (!r.session && (!candidate || r.generation > candidate->generation)) candidate = &r;
    }
    if (candidate) candidate->session = session;
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
    if (kind == Kind::Common && request.command == 1 && reply.scalar == 50) startup(67, 0, policy.pending());
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

// Identity probe implementation lives on the existing BSD worker, never the AM relay.
void network_tick(const char* pairing_code) {
    static std::uint64_t next = 0;
    const auto time = now();
    if (time < next) return;
    next = time + 1000;
    std::uint64_t generation = 0;
    { std::scoped_lock lock(mutex); const auto* a = latest_album(); if (a && a->generation != proven) generation = a->generation; }
    if (!generation) return;
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return;
    const auto flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) { ::close(fd); return; }
    const auto deadline = time + 500;
    const auto pause = [&] { svcSleepThread(5'000'000ULL); };
    sockaddr_in address{}; address.sin_family = AF_INET; address.sin_port = htons(6801); address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const auto line = [&](char* out, std::size_t size) {
        for (std::size_t i = 0; i + 1 < size; ++i) {
            ssize_t count = 0;
            while (now() < deadline && (count = ::recv(fd, out + i, 1, 0)) < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) pause();
            if (now() >= deadline || count != 1) return false;
            if (out[i] == '\n') { out[i] = 0; return true; }
        }
        return false;
    };
    const auto send = [&](const char* text, std::size_t length) {
        std::size_t offset = 0;
        while (offset < length && now() < deadline) {
            const auto count = ::send(fd, text + offset, length - offset, 0);
            if (count > 0) offset += count;
            else if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) pause();
            else return false;
        }
        return offset == length;
    };
    char data[8192]{};
    bool ok = ::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0;
    if (!ok && errno == EINPROGRESS) {
        pollfd pending{fd, POLLOUT, 0};
        while (now() < deadline && ::poll(&pending, 1, 5) == 0) {}
        int error = -1; socklen_t size = sizeof(error);
        ok = now() < deadline && ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &size) == 0 && error == 0;
    }
    if (ok) ok = line(data, sizeof(data));
    char auth[80]; const int count = std::snprintf(auth, sizeof(auth), "auth %s\n", pairing_code);
    if (ok) ok = send(auth, count) && line(data, sizeof(data)) && std::strncmp(data, "+OK", 3) == 0;
    if (ok) ok = send("state 1\n", 8) && line(data, sizeof(data));
    ::close(fd);
    // Current Breeze emits its version as the first root property. Fail closed
    // for different schemas, busy replies, events and unauthenticated endpoints.
    ok = ok && is_breeze_state(data);
    if (ok) {
        std::scoped_lock lock(mutex); const auto* a = latest_album();
        if (a && a->generation == generation) proven = generation;
    }
}
}
