// SPDX-License-Identifier: GPL-2.0-only
#include "trace.hpp"
#include "home_bridge.hpp"
#include "boot.hpp"
#include "launch_bridge.hpp"
#include <algorithm>

using Session = ams::sf::hipc::ServerSession;
using Context = ams::sf::cmif::ServiceDispatchContext;
// Pin the upstream ABI and disable LTO in the library build. The final map and
// disassembly are checked by verify.py to ensure both forwarding call sites
// actually use this observer, not an inlined/unwrapped copy.
#define FORWARD_SYMBOL "_ZNK3ams2sf4hipc13ServerSession14ForwardRequestERKNS0_4cmif22ServiceDispatchContextE"
extern "C" ams::Result real_forward(const Session*, const Context&) asm("__real_" FORWARD_SYMBOL);
extern "C" ams::Result observe_forward(const Session*, const Context&) asm("__wrap_" FORWARD_SYMBOL);
namespace {
struct Object { const Session* session{}; std::uint32_t id{}; observer::Kind kind{}; };
Object objects[2048];
observer::Kind kind(const Session* session, const observer::Request& r, bool root_open) {
    for (const auto& o : objects) if (o.session == session && o.id == r.object) return o.kind;
    return root_open ? observer::Kind::Root : observer::Kind::Unknown;
}
void remember(const Session* session, std::uint32_t id, observer::Kind k) {
    for (auto& o : objects) if ((o.session == session && o.id == id) || o.session == nullptr) { o = {session, id, k}; return; }
}
void forget(const Session* session, std::uint32_t id) {
    for (auto& o : objects) if (o.session == session && o.id == id) o = {};
}
struct Bytes { const std::uint8_t* p{}; std::size_t size{}; };
Bytes raw(const void* buffer, std::size_t size, const std::uint32_t* words, std::size_t count) {
    const auto start = reinterpret_cast<std::uintptr_t>(buffer);
    const auto address = reinterpret_cast<std::uintptr_t>(words);
    if (address < start || address - start > size || count > (size - (address - start)) / 4 || count < 4) return {};
    const auto aligned = (address + 15) & ~std::uintptr_t(15);
    if (aligned < address || aligned - address > count * 4) return {};
    // Match the upstream server's conservative CMIF padding bound.
    return {reinterpret_cast<const std::uint8_t*>(aligned), count * 4 - 16};
}
}
extern "C" ams::Result observe_forward(const Session* session, const Context& ctx) {
    const auto in = raw(ctx.in_message_buffer.GetPointer(), ctx.in_message_buffer.GetSize(), ctx.request.data.data_words, ctx.request.meta.num_data_words);
    const auto request = observer::request(in.p, in.size);
    if (request.valid && observer::root_open(request.command, ctx.request.meta.send_pid, ctx.request.meta.num_copy_handles)) {
        observer::home::bind_root(session, ctx.request.pid);
        observer::launch::bind_root(session, ctx.request.pid);
    }
    const auto interface = request.valid ? kind(session, request,
        observer::root_open(request.command, ctx.request.meta.send_pid, ctx.request.meta.num_copy_handles)) : observer::Kind::Unknown;
    const auto child_kind = observer::returned_kind(interface, request.command);
    observer::Record record;
    record.session = reinterpret_cast<std::uintptr_t>(session);
    record.object = request.object; record.command = request.command; record.kind = interface;
    // Only log defined numeric arguments; never dump arbitrary applet payloads.
    if ((interface == observer::Kind::LibCreator || interface == observer::Kind::AppCreator) && request.command == 0) {
        record.argument = request.argument;
        record.scalar = request.argument_high; // mode or upper half of title ID
    }
    record.flags = (request.domain ? 1 : 0) | (request.valid ? 2 : 0) | (request.closing ? 4 : 0);
    const std::uint8_t* storage_output = nullptr;
    if (interface == observer::Kind::StorageAccessor && request.command == 11 && request.argument == 0 && request.argument_high == 0 && ctx.request.meta.num_recv_buffers == 1) {
        const auto& descriptor = ctx.request.data.recv_buffers[0];
        if (hipcGetBufferSize(&descriptor) >= 16) storage_output = static_cast<const std::uint8_t*>(hipcGetBufferAddress(&descriptor));
    }

    observer::launch::before_forward(session, interface, request);
    // All application commands are forwarded exactly once. qlaunch supplies
    // its own user parameter after the native user-aware debug launch request.
    const auto transport = real_forward(session, ctx);
    record.transport = transport.GetValue();
    if (R_SUCCEEDED(transport) && request.closing) {
        observer::home::observe(session, interface, request, {});
        observer::launch::observe(session, interface, request, {});
        forget(session, request.object);
    }
    if (R_SUCCEEDED(transport) && request.valid && !request.closing) {
        const auto response = hipcParseResponse(ams::sf::hipc::GetMessageBufferOnTls());
        const auto bytes = raw(ams::sf::hipc::GetMessageBufferOnTls(), 0x100, response.data_words, response.num_data_words);
        const auto reply = observer::reply(bytes.p, bytes.size, request.domain, child_kind != observer::Kind::Unknown);
        if (observer::home::is_qlaunch(session)) observer::observe_boot(interface, request, reply);
        observer::home::observe(session, interface, request, reply);
        observer::launch::observe(session, interface, request, reply);
        if (reply.valid) {
            record.flags |= 8; record.result = reply.result;
            if (interface == observer::Kind::Common && request.command == 1 && reply.result == 0) record.scalar = reply.scalar;
            if (interface == observer::Kind::Common && request.command == 1 && reply.result == 0 && reply.scalar == 20 &&
                observer::home::consume_home(session)) {
                // Only the explicitly armed, live Breeze/game pair consumes
                // short HOME. Unarmed/failure/other messages keep stock bytes.
                auto* payload = const_cast<std::uint8_t*>(bytes.p) + (request.domain ? 16 : 0);
                const std::uint32_t no_message = (3u << 9) | 128u, zero = 0;
                std::memcpy(payload + 8, &no_message, 4);
                std::memcpy(payload + 16, &zero, 4);
                record.flags |= 0x400;
            }
            if (reply.object && child_kind != observer::Kind::Unknown) {
                remember(session, reply.object, child_kind); record.child = reply.object;
            }
            if (reply.result == 0 && interface == observer::Kind::StorageAccessor && request.command == 11 && request.argument == 0 && request.argument_high == 0) {
                // Read only the SAMS header already returned to qlaunch. No
                // additional storage Read/Pop is issued. Pointer outputs must
                // lie inside the relay's own bounded pointer buffer.
                if (!storage_output && response.num_statics == 1) {
                    const auto& descriptor = response.statics[0];
                    const auto p = reinterpret_cast<std::uintptr_t>(hipcGetStaticAddress(&descriptor));
                    const auto base = ctx.pointer_buffer.GetAddress();
                    const auto size = ctx.pointer_buffer.GetSize();
                    if (size >= 16 && descriptor.size >= 16 && p >= base && p - base <= size - 16) storage_output = reinterpret_cast<const std::uint8_t*>(p);
                }
                if (storage_output) {
                    const auto header = observer::channel_header(storage_output, 16);
                    if (header.magic) {
                        record.scalar = header.message;
                        record.argument = header.argument;
                        record.flags |= header.magic == 0x534d4153 ? 0x40 : 0x80;
                    }
                }
            }
            if (interface == observer::Kind::Common && request.command == 1 && reply.result == ((3u << 9) | 128u)) return transport;
        }
        if (interface == observer::Kind::Root) remember(session, request.object, interface);
        if (response.num_move_handles && child_kind != observer::Kind::Unknown) record.flags |= 0x20; // child-session coverage gap
    }
    observer::push(record); // fixed ring only; no filesystem IPC on this TLS
    return transport;
}
