// SPDX-License-Identifier: GPL-2.0-only
#include "trace.hpp"
#include "remote_nro_server.hpp"
#include "boot.hpp"
#include <cstdio>
#include <cerrno>

namespace observer {
namespace {
ams::os::ThreadType remote_init_thread;
alignas(0x1000) std::uint8_t remote_init_stack[0x8000];
void initialize_remote(void*) {
    startup(11);
    // libnx's devoptab is used only by the separate remote-control workers.
    // Never wait for SD or networking on the AM forwarding thread.
    for (;;) {
        const ::Result result = fsInitialize();
        startup(12, result);
        if (R_SUCCEEDED(result)) {
            const auto mounted = fsdevMountSdmc();
            startup(13, mounted);
            if (R_SUCCEEDED(mounted)) break;
            fsExit();
        }
        ams::os::SleepThread(ams::TimeSpan::FromSeconds(1));
    }
    errno = 0;
    FILE* probe = std::fopen("sdmc:/config/NSHome/skip-lock-screen.flag", "rb");
    startup(15, probe ? 0 : errno);
    if (probe) std::fclose(probe);
    // Missing/disabled Breeze PC-connect settings mean no listener. The wire
    // greeting stays SWITCHU 1 for compatibility with the existing PC client.
    startup(14, nshome::remote::start(nullptr) ? 0 : 1);
    configure_boot(); // Does not block the separate 6802 worker or AM loop.
    startup_input_once();
}
}
void start_remote() {
    const auto created = ams::os::CreateThread(&remote_init_thread, initialize_remote,
            nullptr, remote_init_stack, sizeof(remote_init_stack), 17, 3);
    startup(10, created.GetValue());
    if (R_SUCCEEDED(created)) {
        ams::os::SetThreadNamePointer(&remote_init_thread, "nshome-remote-init");
        ams::os::StartThread(&remote_init_thread);
    }
}
}
