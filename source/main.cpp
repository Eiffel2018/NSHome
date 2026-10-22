// SPDX-License-Identifier: GPL-2.0-only
#include "trace.hpp"
#include "home_bridge.hpp"
#include "launch_bridge.hpp"
#include "pm_stop.hpp"
#include <cstdlib>

// Commands normally relay unchanged. The launch wrapper scopes PC launch handling.
#define OBSERVER_COMMANDS(C,H)
AMS_SF_DEFINE_MITM_INTERFACE(observer_sf, IAmObserver, OBSERVER_COMMANDS, 0x4e53414d);
namespace observer {
void start_remote();
constexpr ams::ncm::ProgramId ProgramId{0x00ff4e5348414d01ull};
class Service : public ams::sf::MitmServiceImplBase {
public:
    using ams::sf::MitmServiceImplBase::MitmServiceImplBase;
    static bool ShouldMitm(const ams::sm::MitmProcessInfo& p) {
        return p.program_id == ams::ncm::SystemAppletId::Qlaunch || p.program_id.value == 0x010000000000100dull;
    }
};
struct Options : ams::sf::hipc::DefaultServerManagerOptions {
    static constexpr std::size_t PointerBufferSize = 0x1000;
    static constexpr std::size_t MaxDomains = 16;
    static constexpr std::size_t MaxDomainObjects = 1024;
    static constexpr bool CanManageMitmServers = true;
};
class Manager final : public ams::sf::hipc::ServerManager<1, Options, 32> {
    ams::Result OnNeedsToAccept(int, Server* server) override {
        std::shared_ptr<::Service> forward;
        ams::sm::MitmProcessInfo client{};
        server->AcknowledgeMitmSession(&forward, &client);
        home::register_root(forward, client.process_id.value, client.program_id == ams::ncm::SystemAppletId::Qlaunch);
        launch::register_root(forward, client.process_id.value, client.program_id == ams::ncm::SystemAppletId::Qlaunch);
        accepted(client.process_id.value);
        return AcceptMitmImpl(server, ams::sf::CreateSharedObjectEmplaced<observer_sf::IAmObserver, Service>(decltype(forward)(forward), client), forward);
    }
public:
    ams::Result install() { return RegisterMitmServer<Service>(0, ams::sm::ServiceName::Encode("appletAE")); }
};
}
namespace ams {
ncm::ProgramId CurrentProgramId = observer::ProgramId;
namespace init {
    alignas(0x1000) constinit std::uint8_t heap[0x400000];
    void InitializeSystemModule() { InitializeAllocator(heap, sizeof(heap)); R_ABORT_UNLESS(sm::Initialize()); }
    void FinalizeSystemModule() {}
    void Startup() {}
}
extern "C" void* __libnx_alloc(std::size_t size) { return std::malloc(size); }
extern "C" void* __libnx_aligned_alloc(std::size_t alignment, std::size_t size) { return std::aligned_alloc(alignment, size); }
extern "C" void __libnx_free(void* pointer) { std::free(pointer); }
void Main() {
    observer::start_log();
    observer::startup(1);
    // Remote diagnostics/control must not wait for AM registration to finish.
    observer::start_remote();
    // Static storage: keep the server tables off the main thread stack.
    static observer::Manager manager;
    observer::startup(2);
    const auto installed = manager.install();
    observer::startup(3, installed.GetValue());
    R_ABORT_UNLESS(installed);
    observer::startup(4);
    observer::start_pm_stop();
    manager.LoopProcess();
}
}
