// SPDX-License-Identifier: GPL-2.0-only
#include "trace.hpp"
#include "boot.hpp"
#include "boot_gate.hpp"
#include "boot_screen.hpp"
#include "remote_nro_input.hpp"
#include "remote_nro_touch.hpp"
#include "remote_nro_server.hpp"
#include "remote_nro_capture.hpp"
#include <cstdlib>
#include <cstdio>
#include <cerrno>

namespace observer {
namespace {
BootInputGate input_gate;
std::uint64_t now_ms() { return armTicksToNs(armGetSystemTick()) / 1000000; }
bool input_allowed() { return input_gate.may_send(now_ms()) && boot_screen_ready() && input_gate.may_send(now_ms()); }
}
void observe_boot(Kind kind, const Request& request, const Reply& reply) {
    if (!reply.valid || reply.result || request.closing) return;
    if (kind == Kind::Home && request.command == 11) input_gate.lock();
    if (kind == Kind::Common && request.command == 1) {
        if (reply.scalar == 1) input_gate.foreground(now_ms());
        if (reply.scalar == 25) input_gate.cancel();
        if (reply.scalar == 26) nshome::remote::notifyWake();
    }
    // UnlockForeground marks completed entry into the normal HOME menu.
    // An applet/game launch is also a reason not to send a delayed boot key.
    if ((kind == Kind::Home && request.command == 12) ||
        (kind == Kind::LibCreator && (request.command == 0 || request.command == 3)) ||
        (kind == Kind::AppCreator && (request.command == 0 || request.command == 1))) input_gate.cancel();
}
void startup_input_once() {
    FILE* enabled = std::fopen("sdmc:/config/NSHome/boot-touch-once.flag", "rb");
    const bool touch = enabled != nullptr;
    if (!enabled) enabled = std::fopen("sdmc:/config/NSHome/boot-a-once.flag", "rb");
    startup(30, 0, touch ? 2 : enabled != nullptr);
    if (!enabled) return;
    std::fclose(enabled);
    const auto deadline = now_ms() + 60000;
    // Rapid caps:sc close/reopen returned sm::OutOfSessions on the final
    // pre-contact check on device. Hold one reference through all checks.
    struct CaptureSession {
        bool open{};
        ~CaptureSession() { if (open) capsscExit(); }
    } capture;
    unsigned ready_frames = 0;
    bool saved_frame = false;
    while (now_ms() < deadline) {
        if (input_gate.window(now_ms()) && !capture.open) {
            const auto rc = capsscInitialize();
            startup(35, rc);
            if (R_FAILED(rc)) return;
            capture.open = true;
        }
        if (!saved_frame && input_gate.window(now_ms())) {
            saved_frame = true;
            void* jpeg = nullptr;
            std::size_t size = 0;
            const auto rc = nshome::remote_capture::captureJpeg(&jpeg, &size);
            if (R_SUCCEEDED(rc)) {
                FILE* file = std::fopen("sdmc:/config/NSHome/boot-screen.jpg", "wb");
                if (file) { (void)std::fwrite(jpeg, 1, size, file); std::fclose(file); }
                std::free(jpeg);
            }
            startup(34, rc);
        }
        if (input_gate.window(now_ms())) ready_frames = boot_screen_ready() ? ready_frames + 1 : 0;
        else ready_frames = 0;
        if (ready_frames >= 2 && input_gate.claim(now_ms())) {
            bool attempted = false;
            const auto rc = touch
                ? nshome::remote_touch::gestureIf(820, 346, 100, input_allowed, &attempted)
                : nshome::remote_input::pressButtonIf(HidNpadButton_A, 100, input_allowed, &attempted);
            startup(31, rc, attempted);
            return; // Never replay, including ambiguous publication failures.
        }
        ams::os::SleepThread(ams::TimeSpan::FromMilliSeconds(200));
    }
    startup(32); // No eligible boot window; never send after a late wake.
}
void configure_boot() {
    FILE* enabled = std::fopen("sdmc:/config/NSHome/skip-lock-screen.flag", "rb");
    startup(20, 0, enabled != nullptr);
    if (!enabled) return;
    std::fclose(enabled);
    const ::Result initialized = setsysInitialize();
    startup(21, initialized);
    if (R_FAILED(initialized)) return;
    bool before = false;
    ::Result result = setsysGetLockScreenFlag(&before);
    startup(22, result, before);
    if (R_SUCCEEDED(result)) {
        constexpr const char* original = "sdmc:/config/NSHome/boot-lock.previous";
        FILE* previous = std::fopen(original, "rb");
        if (previous) {
            std::fclose(previous);
        } else {
            FILE* file = std::fopen(original, "wb");
            bool saved = false;
            if (file) {
                saved = std::fputc(before ? '1' : '0', file) != EOF;
                if (std::fclose(file) != 0) saved = false;
            }
            if (!saved || R_FAILED(fsdevCommitDevice("sdmc"))) {
                startup(25, static_cast<std::uint32_t>(errno ? errno : EIO));
                setsysExit();
                return;
            }
        }
        if (before) {
            result = setsysSetLockScreenFlag(false);
            startup(23, result);
        }
        if (R_SUCCEEDED(result)) {
            bool after = true;
            result = setsysGetLockScreenFlag(&after);
            startup(24, result, after);
        }
    }
    setsysExit();
}
}
