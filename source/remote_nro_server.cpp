// SPDX-License-Identifier: GPL-2.0-only
// Adapted from Eiffel2018/SwitchU 546903f6 (see NOTICE.md).
#include "remote_nro_server.hpp"

#include "remote_nro_protocol.hpp"
#include "remote_session.hpp"
#include <array>
#include "remote_nro_input.hpp"
#include "remote_nro_capture.hpp"
#include "remote_nro_touch.hpp"
#include "remote_nro_titles.hpp"
#include "home_bridge.hpp"
#include "launch_bridge.hpp"
#include <switch.h>
#include <stratosphere.hpp>

#include <arpa/inet.h>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>

namespace nshome::remote {
namespace {

constexpr const char* kConfigPath = "sdmc:/switch/Breeze/config.ini";
// Breeze PC connect owns 6801; SwitchU uses its separate fixed endpoint.
constexpr std::uint16_t kDefaultPort = 6802;
constexpr std::size_t kMaxCodeLength = 64;
constexpr const char* kDiagnosticPath = "sdmc:/config/NSHome/remote.log";
constexpr long kDiagnosticMaxBytes = 128 * 1024;
std::atomic<bool> g_inputBlocked{false};

struct Settings {
    std::string code;
};

std::mutex g_mutex;
ams::os::ThreadType g_thread{};
alignas(0x1000) std::uint8_t g_stack[0x10000];
std::atomic<bool> g_running{false};
std::atomic<std::uint64_t> g_wakeEpoch{0};
bool g_started = false;
bool g_pending = false;
bool g_busy = false;
std::uint64_t g_nextId = 1;
std::uint64_t g_currentId = 0;
std::uint64_t g_bootTick = 0;
std::string g_path;
RequestKind g_kind = RequestKind::Nro;
std::uint64_t g_titleId = 0;
std::string g_state = "idle";
std::uint32_t g_result = 0;
std::string g_code;
void (*g_wake)() = nullptr;

void diagnostic(const char* event, int fd = -1, int error = 0, Result bsdResult = 0) {
    (void)::mkdir("sdmc:/config", 0777);
    (void)::mkdir("sdmc:/config/NSHome", 0777);
    FILE* file = std::fopen(kDiagnosticPath, "a+");
    if (!file)
        return;
    if (std::fseek(file, 0, SEEK_END) == 0 && std::ftell(file) > kDiagnosticMaxBytes) {
        (void)std::fclose(file);
        file = std::fopen(kDiagnosticPath, "w");
        if (!file)
            return;
    }
    std::fprintf(file, "[%s] event=%s fd=%d errno=%d bsd=0x%08X\n",
                 kRemoteChannelVersionMarker, event ? event : "unknown", fd,
                 error, static_cast<unsigned int>(bsdResult));
    (void)std::fclose(file);
}

std::string_view trim(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t'
                              || value.front() == '\r' || value.front() == '\n'))
        value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t'
                              || value.back() == '\r' || value.back() == '\n'))
        value.remove_suffix(1);
    return value;
}

unsigned char asciiLower(unsigned char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<unsigned char>(c + ('a' - 'A')) : c;
}

bool equalAsciiCaseInsensitive(std::string_view left, std::string_view right) {
    if (left.size() != right.size())
        return false;
    for (std::size_t i = 0; i < left.size(); ++i) {
        if (asciiLower(static_cast<unsigned char>(left[i]))
            != asciiLower(static_cast<unsigned char>(right[i])))
            return false;
    }
    return true;
}

bool validCode(std::string_view code) {
    if (code.size() < 4 || code.size() > kMaxCodeLength)
        return false;
    for (unsigned char c : code) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
              || (c >= '0' && c <= '9') || c == '-' || c == '_'))
            return false;
    }
    return true;
}

bool readSettings(Settings& settings) {
    std::ifstream file(kConfigPath);
    if (!file)
        return false;

    bool inSection = false;
    bool foundSection = false;
    bool foundEnable = false;
    bool enabled = false;
    bool foundCode = false;
    std::string code;
    std::string line;
    while (std::getline(file, line)) {
        if (line.size() > 1024)
            return false;
        std::string_view current = trim(line);
        if (current.empty() || current.front() == '#' || current.front() == ';')
            continue;
        if (current.front() == '[' && current.back() == ']') {
            const std::string_view section = trim(current.substr(1, current.size() - 2));
            inSection = equalAsciiCaseInsensitive(section, "PC connect");
            foundSection = foundSection || inSection;
            continue;
        }
        if (!inSection)
            continue;
        const std::size_t equals = current.find('=');
        if (equals == std::string_view::npos)
            continue;
        const std::string_view key = trim(current.substr(0, equals));
        std::string_view value = trim(current.substr(equals + 1));
        const std::size_t comment = value.find_first_of(";#");
        if (comment != std::string_view::npos)
            value = trim(value.substr(0, comment));

        if (equalAsciiCaseInsensitive(key, "enable")) {
            foundEnable = true;
            enabled = equalAsciiCaseInsensitive(value, "true") || value == "1"
                || equalAsciiCaseInsensitive(value, "yes")
                || equalAsciiCaseInsensitive(value, "on");
        } else if (equalAsciiCaseInsensitive(key, "code")) {
            foundCode = true;
            code.assign(value);
        }
    }
    if (!file.eof() || !foundSection || !foundEnable || !enabled || !foundCode
        || !validCode(code))
        return false;

    settings.code = std::move(code);
    return true;
}

void closeSocket(int fd) {
    if (fd >= 0)
        ::close(fd);
}

void sleepPoll() {
    svcSleepThread(50'000'000ULL);
}

bool sendAll(int fd, std::string_view value, const char* stage) {
    std::size_t sent = 0;
    unsigned int polls = 0;
    const u64 started = armGetSystemTick();
    while (sent < value.size() && g_running.load() && polls < 40 &&
           armTicksToNs(armGetSystemTick() - started) < 2'000'000'000ULL) {
        const ssize_t count = ::send(fd, value.data() + sent, value.size() - sent, 0);
        if (count > 0) {
            sent += static_cast<std::size_t>(count);
            polls = 0;
        } else if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
            ++polls;
            svcSleepThread(25'000'000ULL);
        } else {
            const int error = count < 0 ? errno : EPIPE;
            diagnostic(stage, fd, error, count < 0 ? socketGetLastResult() : 0);
            return false;
        }
    }
    if (sent == value.size())
        return true;
    diagnostic(stage, fd, g_running.load() ? ETIMEDOUT : ECANCELED,
               g_running.load() ? socketGetLastResult() : 0);
    return false;
}

void sendStatus(int fd) {
    std::uint64_t id;
    std::string state;
    std::uint32_t result;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        id = g_currentId;
        state = g_state;
        result = g_result;
    }
    char buffer[192];
    const int count = std::snprintf(buffer, sizeof(buffer),
        "OK status id=%llu state=%s rc=0x%08X build=%s boot=%llu\n",
        static_cast<unsigned long long>(id), state.c_str(), result, kBuildVersion,
        static_cast<unsigned long long>(g_bootTick));
    if (count > 0 && static_cast<std::size_t>(count) < sizeof(buffer))
        (void)sendAll(fd, std::string_view(buffer, static_cast<std::size_t>(count)),
                      "status-response");
}

void answerError(int fd, const char* reason) {
    char buffer[64];
    const int count = std::snprintf(buffer, sizeof(buffer), "ERR %s\n", reason);
    if (count > 0 && static_cast<std::size_t>(count) < sizeof(buffer))
        (void)sendAll(fd, std::string_view(buffer, static_cast<std::size_t>(count)),
                      "protocol-error-response");
}

bool queueLaunch(int fd, const std::string& path, std::uint64_t titleId = 0, bool stop = false) {
    if (!g_wake)
        return sendAll(fd, "ERR launch-bridge-unavailable\n", "launch-unavailable");
    std::uint64_t id = 0;
    void (*wake)() = nullptr;
    bool isBusy = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        isBusy = g_busy;
        if (!isBusy) {
            id = g_nextId++;
            if (g_nextId == 0)
                g_nextId = 1;
            g_currentId = id;
            g_path = path;
            g_kind = stop ? RequestKind::StopGame : titleId ? RequestKind::StartGame : RequestKind::Nro;
            g_titleId = titleId;
            g_state = "queued";
            g_result = 0;
            g_pending = true;
            g_busy = true;
            wake = g_wake;
        }
    }
    if (isBusy)
        return sendAll(fd, "ERR busy\n", "busy-response");
    char response[64];
    const int count = std::snprintf(response, sizeof(response), "OK queued %llu\n",
                                    static_cast<unsigned long long>(id));
    const bool sent = count > 0 && static_cast<std::size_t>(count) < sizeof(response)
        && sendAll(fd, std::string_view(response, static_cast<std::size_t>(count)),
                   "launch-response");
    if (wake)
        wake();
    return sent;
}

const char* dispatchClient(int fd, const std::string& line, const Command& command) {
        if (line.substr(0, 5) == "home ") {
            const auto response = observer::home::command(line);
            if (!sendAll(fd, response, "home-response")) return "home-response-failed";
        } else if (command.kind == CommandKind::Status) {
            sendStatus(fd);
        } else if (command.kind == CommandKind::TitlesRefresh || command.kind == CommandKind::TitlesStatus || command.kind == CommandKind::TitlesPage) {
            if (!sendAll(fd,nshome::titles::command(command),"titles-response")) return "titles-response-failed";
        } else if (command.kind == CommandKind::Controller) {
            Result rc = 0;
            if (command.argument == "off") {
                g_inputBlocked.store(true);
                rc = nshome::remote_input::shutdown();
                const Result touchRc = nshome::remote_touch::shutdown();
                if (R_SUCCEEDED(rc)) rc = touchRc;
                diagnostic("controller-blocked", fd, 0, rc);
            } else if (command.argument == "on") {
                g_inputBlocked.store(false);
                diagnostic("controller-enabled", fd);
            }
            if (R_FAILED(rc)) {
                char response[80];
                const int count = std::snprintf(response, sizeof(response),
                    "ERR controller-cleanup rc=0x%08X\n", rc);
                if (count <= 0 || !sendAll(fd, std::string_view(response, count), "controller-response"))
                    return "controller-response-failed";
            } else if (!sendAll(fd, g_inputBlocked.load()
                ? "OK controller disabled\n" : "OK controller enabled\n", "controller-response")) {
                return "controller-response-failed";
            }
        } else if (command.kind == CommandKind::Capture) {
            void* image = nullptr;
            std::size_t size = 0;
            const ViLayerStack stack = command.argument.empty() ? ViLayerStack_Screenshot
                : static_cast<ViLayerStack>(command.argument[0] - '0');
            const Result rc = nshome::remote_capture::captureJpeg(&image, &size, stack);
            char response[128];
            if (R_FAILED(rc)) {
                const int count = std::snprintf(response, sizeof(response),
                    "ERR capture rc=0x%08X\n", rc);
                diagnostic("capture-failed", fd, 0, rc);
                if (count > 0)
                    (void)sendAll(fd, std::string_view(response, count), "capture-error");
            } else {
                const int count = std::snprintf(response, sizeof(response),
                    "OK capture bytes=%zu width=1280 height=720 format=jpeg\n", size);
                const bool sent = count > 0 &&
                    sendAll(fd, std::string_view(response, count), "capture-header") &&
                    sendAll(fd, std::string_view(static_cast<const char*>(image), size), "capture-image");
                std::free(image);
                if (!sent)
                    return "capture-response-failed";
                // Avoid one SD log write per preview frame.
            }
        } else if (command.kind == CommandKind::Launch) {
            if (!queueLaunch(fd, command.argument))
                return "launch-response-failed";
        } else if (command.kind == CommandKind::StartGame || command.kind == CommandKind::StopGame) {
            if (!queueLaunch(fd, {}, command.titleId, command.kind == CommandKind::StopGame))
                return "game-start-response-failed";
        } else if (command.kind == CommandKind::Press) {
            if (g_inputBlocked.load()) {
                answerError(fd, "input-disabled");
                return "input-blocked";
            }
            // Runs on the dedicated server worker, independent of the NRO.
            // The bounded call releases our own virtual controller before replying.
            const Result rc = nshome::remote_input::pressButton(
                command.buttonMask, command.holdMs);
            char response[96];
            const int count = R_SUCCEEDED(rc)
                ? std::snprintf(response, sizeof(response), "OK pressed %s ms=%u\n",
                                command.argument.c_str(), command.holdMs)
                : std::snprintf(response, sizeof(response), "ERR input rc=0x%08X\n", rc);
            diagnostic(R_SUCCEEDED(rc) ? "input-released" : "input-failed", fd, 0, rc);
            if (count <= 0 || !sendAll(fd, std::string_view(response, count), "input-response"))
                return "input-response-failed";
        } else if (command.kind == CommandKind::Touch) {
            if (g_inputBlocked.load()) {
                answerError(fd, "input-disabled");
                return "input-blocked";
            }
            // One complete gesture is serialized on this worker. No gamepad,
            // extra controller slot, or client-held touch remains afterward.
            const Result rc = nshome::remote_touch::gesture(
                command.stickX, command.stickY, command.holdMs,
                command.argument == "double", command.gapMs);
            char response[128];
            const int count = R_SUCCEEDED(rc)
                ? std::snprintf(response, sizeof(response),
                    "OK touch %s x=%d y=%d ms=%u gap=%u\n", command.argument.c_str(),
                    command.stickX, command.stickY, command.holdMs, command.gapMs)
                : std::snprintf(response, sizeof(response), "ERR touch rc=0x%08X\n", rc);
            diagnostic(R_SUCCEEDED(rc) ? "touch-released" : "touch-failed", fd, 0, rc);
            if (count <= 0 || !sendAll(fd, std::string_view(response, count), "touch-response"))
                return "touch-response-failed";
        } else if (command.kind == CommandKind::Stick) {
            if (g_inputBlocked.load()) {
                answerError(fd, "input-disabled");
                return "input-blocked";
            }
            const Result rc = nshome::remote_input::moveStick(command.argument == "R",
                command.stickX, command.stickY, command.holdMs);
            char response[112];
            const int count = R_SUCCEEDED(rc)
                ? std::snprintf(response, sizeof(response), "OK stick %s x=%d y=%d ms=%u\n",
                    command.argument.c_str(), command.stickX, command.stickY, command.holdMs)
                : std::snprintf(response, sizeof(response), "ERR input rc=0x%08X\n", rc);
            diagnostic(R_SUCCEEDED(rc) ? "stick-centered" : "stick-failed", fd, 0, rc);
            if (count <= 0 || !sendAll(fd, std::string_view(response, count), "stick-response"))
                return "stick-response-failed";
        } else if (command.kind == CommandKind::InvalidPath) {
            answerError(fd, "invalid-path");
        } else {
            answerError(fd, "invalid-command");
        }
    return "request-complete";
}

std::uint64_t monotonicMs() { return armTicksToNs(armGetSystemTick()) / 1000000; }
struct ClientSlot { int fd = -1; Session session; };
void closeClient(ClientSlot& slot, const char* reason) {
    diagnostic(reason, slot.fd);
    closeSocket(slot.fd); slot.fd = -1;
}
void pollClient(ClientSlot& slot) {
    if (slot.fd < 0) return;
    if (slot.session.expired(monotonicMs())) { closeClient(slot,"client-timeout"); return; }
    char bytes[128];
    // Bounded per peer, including a sender that continually supplies bytes.
    const auto count = ::recv(slot.fd, bytes, sizeof(bytes), 0);
    if (!count) { closeClient(slot,"peer-closed"); return; }
    if (count < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) closeClient(slot,"read-failed");
        return;
    }
    for (ssize_t i=0; i<count; ++i) {
        const auto result = slot.session.feed(bytes[i]);
        if (result == Session::Feed::TooLong) { closeClient(slot,"command-too-long"); return; }
        if (result != Session::Feed::Line) continue;
        const auto command = parseCommand(slot.session.line);
        if (!slot.session.authenticated) {
            if (command.kind != CommandKind::Auth || !constantTimeEqual(command.argument,g_code)) {
                answerError(slot.fd,"auth"); closeClient(slot,"auth-failed"); return;
            }
            if (!sendAll(slot.fd,"OK auth\n","auth-ok-response")) { closeClient(slot,"auth-response-failed"); return; }
            slot.session.accept_auth(monotonicMs());
        } else {
            // Mutations remain serialized, one operation per authenticated socket.
            closeClient(slot,dispatchClient(slot.fd,slot.session.line,command)); return;
        }
    }
}

int openListener() {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        diagnostic("socket-create-failed", -1, errno, socketGetLastResult());
        return -1;
    }
    int yes = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
        diagnostic("listener-reuseaddr-failed", fd, errno, socketGetLastResult());
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        diagnostic("listener-fcntl-get-failed", fd, errno);
        closeSocket(fd);
        return -1;
    }
    if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        diagnostic("listener-fcntl-set-failed", fd, errno);
        closeSocket(fd);
        return -1;
    }
    sockaddr_in address{};
    address.sin_len = sizeof(address);
    address.sin_family = AF_INET;
    address.sin_port = htons(kDefaultPort);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0
        || ::listen(fd, 8) < 0) {
        const int error = errno;
        diagnostic("listener-bind-or-listen-failed", fd, error, socketGetLastResult());
        closeSocket(fd);
        return -1;
    }
    return fd;
}

void serverThread(void*) {
    bool socketReady = false;
    while (g_running.load()) {
        if (!socketReady) {
            const SocketInitConfig socketConfig = {
                .tcp_tx_buf_size = 0x8000,
                .tcp_rx_buf_size = 0x8000,
                .tcp_tx_buf_max_size = 0x10000,
                .tcp_rx_buf_max_size = 0x10000,
                .udp_tx_buf_size = 0,
                .udp_rx_buf_size = 0,
                .sb_efficiency = 4,
                .num_bsd_sessions = 3,
                // This worker is a system module, without an applet lifecycle.
                .bsd_service_type = BsdServiceType_System,
            };
            const Result rc = socketInitialize(&socketConfig);
            if (R_FAILED(rc)) {
                diagnostic("socket-init-failed", -1, 0, rc);
                for (unsigned int i = 0; i < 40 && g_running.load(); ++i)
                    svcSleepThread(50'000'000ULL);
                continue;
            }
            diagnostic("socket-init-system-ok");
            socketReady = true;
        }

        const auto listenerEpoch = g_wakeEpoch.load();
        int listener = openListener();
        if (listener < 0) {
            for (unsigned int i = 0; i < 40 && g_running.load(); ++i)
                svcSleepThread(50'000'000ULL);
            continue;
        }
        sockaddr_in bound{};
        socklen_t boundLength = sizeof(bound);
        const int boundResult = ::getsockname(listener, reinterpret_cast<sockaddr*>(&bound), &boundLength);
        const int boundError = boundResult < 0 ? errno : 0;
        int listening = 0;
        socklen_t optionLength = sizeof(listening);
        const int optionResult = ::getsockopt(listener, SOL_SOCKET, SO_ACCEPTCONN, &listening, &optionLength);
        char details[128];
        std::snprintf(details, sizeof(details), "listener-bound port=%u address=%08X accept=%d name_rc=%d option_rc=%d",
                      ntohs(bound.sin_port), static_cast<unsigned>(ntohl(bound.sin_addr.s_addr)),
                      listening, boundResult, optionResult);
        diagnostic(details, listener, boundError);
        diagnostic("listener-ready", listener);
        std::array<ClientSlot, 4> clients{};
        bool loggedPoll = false;

        while (g_running.load() && g_wakeEpoch.load() == listenerEpoch) {
            observer::launch::network_tick(g_code.c_str());
            sockaddr_in clientAddress{};
            socklen_t addressLength = sizeof(clientAddress);
            const int client = ::accept(listener,
                reinterpret_cast<sockaddr*>(&clientAddress), &addressLength);
            if (client >= 0) {
                ClientSlot* slot = nullptr;
                for (auto& entry : clients) if (entry.fd < 0) { slot = &entry; break; }
                const int flags = ::fcntl(client,F_GETFL,0);
                if (!slot || flags < 0 || ::fcntl(client,F_SETFL,flags | O_NONBLOCK) < 0 ||
                    !sendAll(client,"SWITCHU 1\n","greeting-send")) closeSocket(client);
                else { slot->fd = client; slot->session.start(monotonicMs()); }
            } else
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                diagnostic("accept-failed", listener, errno, socketGetLastResult());
                break;
            }
            if (!loggedPoll) {
                diagnostic("accept-polling", listener, errno, socketGetLastResult());
                loggedPoll = true;
            }
            for (auto& client : clients) pollClient(client);
            sleepPoll();
        }
        for (auto& client : clients) if (client.fd >= 0) closeClient(client,"listener-reset");
        closeSocket(listener);
        observer::launch::network_reset();
        if (g_wakeEpoch.load() != listenerEpoch) {
            diagnostic("wake-reinitializing-bsd");
            socketExit();
            socketReady = false;
        }
    }
    const Result inputCleanup = nshome::remote_input::shutdown();
    if (R_FAILED(inputCleanup))
        diagnostic("input-cleanup-failed", -1, 0, inputCleanup);
    const Result touchCleanup = nshome::remote_touch::shutdown();
    if (R_FAILED(touchCleanup))
        diagnostic("touch-cleanup-failed", -1, 0, touchCleanup);
    if (socketReady)
        socketExit();
}

} // namespace

void notifyWake() { g_wakeEpoch.fetch_add(1); }

bool start(void (*wake)()) {
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_started)
            return true;
    }

    Settings settings;
    if (!readSettings(settings)) {
        diagnostic("settings-disabled-or-invalid");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_started)
            return true;
        g_code = std::move(settings.code);
        g_bootTick = armGetSystemTick();
        if (!nshome::titles::start(g_bootTick)) diagnostic("titles-worker-unavailable");
        g_wake = wake;
        g_running.store(true);
        const auto created = ams::os::CreateThread(&g_thread, serverThread, nullptr,
                                 g_stack, sizeof(g_stack), 18, 3);
        const Result rc = created.GetValue();
        if (R_FAILED(rc)) {
            diagnostic("worker-create-failed", -1, 0, rc);
            g_running.store(false);
            g_code.clear();
            g_wake = nullptr;
            return false;
        }
        ams::os::SetThreadNamePointer(&g_thread, "nshome-6802");
        ams::os::StartThread(&g_thread);
        g_started = true;
    }
    diagnostic(kRemoteChannelVersionMarker);
    return true;
}

void stop() {
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_started)
            return;
        g_running.store(false);
    }
    // The worker uses nonblocking accept/recv/send and bounded sleeps, so it
    // observes shutdown promptly without closing a descriptor from two threads.
    ams::os::WaitThread(&g_thread);
    ams::os::DestroyThread(&g_thread);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_started = false;
        g_pending = false;
        g_busy = false;
        g_currentId = 0;
        g_path.clear();
        g_kind = RequestKind::Nro;
        g_titleId = 0;
        g_state = "idle";
        g_result = 0;
        g_code.clear();
        g_wake = nullptr;
    }
}

bool take(Request& request) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_pending)
        return false;
    request.id = g_currentId;
    request.path = std::move(g_path);
    request.kind = g_kind;
    request.titleId = g_titleId;
    g_path.clear();
    g_pending = false;
    g_state = "launching";
    return true;
}

bool hasPending() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_pending;
}

bool busy() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_busy;
}

void finish(std::uint64_t id, const char* state, std::uint32_t result) {
    if (!state)
        return;
    const std::string_view requested(state);
    if (requested != "started" && requested != "finished" && requested != "failed")
        return;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_currentId != id)
        return;
    g_state.assign(requested);
    g_result = result;
    if (requested == "started" || requested == "finished" || requested == "failed")
        g_busy = false;
}

} // namespace nshome::remote
