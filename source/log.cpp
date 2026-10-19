// SPDX-License-Identifier: GPL-2.0-only
#include "trace.hpp"
#include <cstdio>
#include <mutex>
#include <cstdlib>

namespace observer {
namespace {
constexpr std::size_t Capacity = 1024, Limit = 4 * 1024 * 1024;
constinit Record ring[Capacity];
constinit std::size_t read_index{}, write_index{}, dropped{};
constinit std::uint64_t sequence{};
constinit ams::os::SdkMutex mutex;
alignas(0x1000) constinit std::uint8_t stack[0x6000];
constinit ams::os::ThreadType thread;
bool append(const char* path, std::size_t offset, const char* bytes, std::size_t size) {
    // Do not hold a write handle across idle periods: external SD readers
    // otherwise cannot open this file, and may observe an uncommitted size.
    for (unsigned attempt = 0; attempt < 20; ++attempt) {
        ams::fs::FileHandle file;
        const auto opened = ams::fs::OpenFile(&file, path, ams::fs::OpenMode_Write | ams::fs::OpenMode_AllowAppend);
        if (R_SUCCEEDED(opened)) {
            const auto written = ams::fs::WriteFile(file, offset, bytes, size, ams::fs::WriteOption::Flush);
            ams::fs::CloseFile(file);
            return R_SUCCEEDED(written);
        }
        ams::os::SleepThread(ams::TimeSpan::FromMilliSeconds(100));
    }
    return false;
}
void writer(void*) {
    // All filesystem IPC runs on this thread's TLS, never in the AM relay.
    ams::fs::InitializeForSystem();
    ams::fs::SetAllocator([](std::size_t size) { return std::malloc(size); }, [](void* p, std::size_t) { std::free(p); });
    if (R_FAILED(ams::fs::MountSdCard("sdmc"))) return;
    (void)ams::fs::CreateDirectory("sdmc:/config");
    (void)ams::fs::CreateDirectory("sdmc:/config/NSHome");
    constexpr const char* path = "sdmc:/config/NSHome/am-observer.log";
    constexpr const char* previous = "sdmc:/config/NSHome/am-observer.previous.log";
    (void)ams::fs::DeleteFile(previous);
    (void)ams::fs::RenameFile(path, previous);
    (void)ams::fs::CreateFile(path, 0);
    ams::fs::FileHandle file;
    if (R_FAILED(ams::fs::OpenFile(&file, path, ams::fs::OpenMode_Write | ams::fs::OpenMode_AllowAppend))) return;
    if (R_FAILED(ams::fs::SetFileSize(file, 0))) { ams::fs::CloseFile(file); return; }
    ams::fs::CloseFile(file);
    const char header[] = "AM Observer 0.1.8; experimental stock-qlaunch PC launch and HOME bridge\ncoverage=root+domain RPC; copied child sessions may bypass tracing\nflags:0x40=SAMS,0x80=BZOV,0x200=startup,0x400=HOME consumed,0x800=HOME result,0x1000=scoped launch replacement\nstartup:60=launch worker,61=native launch (arg=port),62=preselected user,63=application Start,64=Breeze start,65=Breeze identified,67=AM launch-request message\nfields:seq tick session interface object cmd arg transport_rc am_rc scalar child flags dropped\n";
    std::size_t offset = sizeof(header) - 1;
    if (!append(path, 0, header, offset)) return;
    for (;;) {
        Record r; bool available; std::size_t lost;
        {
            std::scoped_lock lock(mutex);
            available = read_index != write_index; lost = dropped;
            if (available) { r = ring[read_index]; read_index = (read_index + 1) % Capacity; }
        }
        if (!available) { ams::os::SleepThread(ams::TimeSpan::FromMilliSeconds(100)); continue; }
        char line[256];
        const int length = std::snprintf(line, sizeof(line), "%llu %llu %llu %s %u %u %u 0x%x 0x%x %u %u 0x%x %zu\n", (unsigned long long)r.seq, (unsigned long long)r.tick, (unsigned long long)r.session, name(r.kind), r.object, r.command, r.argument, r.transport, r.result, r.scalar, r.child, r.flags, lost);
        if (length <= 0 || std::size_t(length) >= sizeof(line)) continue;
        if (offset + std::size_t(length) > Limit) {
            const char end[] = "TRACE_LIMIT_REACHED; relay continues without further disk logging\n";
            (void)append(path, offset, end, sizeof(end) - 1);
            return;
        }
        if (!append(path, offset, line, length)) return;
        offset += length;
    }
}
}
void push(Record r) {
    std::scoped_lock lock(mutex);
    r.seq = ++sequence; r.tick = armGetSystemTick();
    const auto next = (write_index + 1) % Capacity;
    if (next == read_index) { ++dropped; return; }
    ring[write_index] = r; write_index = next;
}
void accepted(std::uint64_t pid) { Record r; r.pid = pid; r.session = pid; r.kind = Kind::Root; r.flags = 0x100; push(r); }
void startup(std::uint32_t stage, std::uint32_t result, std::uint32_t value) { Record r; r.command = stage; r.result = result; r.argument = value; r.flags = 0x200; push(r); }
void start_log() {
    // ams::os uses priority 0..31, translated to Horizon priority +28.
    const auto rc = ams::os::CreateThread(&thread, writer, nullptr, stack, sizeof(stack), 16, 3);
    if (R_SUCCEEDED(rc)) { ams::os::SetThreadNamePointer(&thread, "am-observer-log"); ams::os::StartThread(&thread); }
}
}
