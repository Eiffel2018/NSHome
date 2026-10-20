// SPDX-License-Identifier: GPL-2.0-only
#include "trace.hpp"
#include "pm_stop.hpp"
#define PM_STOP_COMMANDS(C,H) AMS_SF_METHOD_INFO(C,H,1,::ams::Result,TerminateProcess,(u64 pid),(pid))
AMS_SF_DEFINE_MITM_INTERFACE(ams::pm_stop_sf, IPmStop, PM_STOP_COMMANDS, 0x4e53504d);
namespace observer {
namespace {
class PmStop : public ams::sf::MitmServiceImplBase {
public:
    using ams::sf::MitmServiceImplBase::MitmServiceImplBase;
    static bool ShouldMitm(const ams::sm::MitmProcessInfo& p) {
        return p.program_id.value == 0x010000000000100dull;
    }
    ams::Result TerminateProcess(u64 pid) {
        std::uint32_t result=0;
        if (!launch::compatibility_stop(m_client_info.process_id.value,pid,result))
            return ams::sm::mitm::ResultShouldForwardToSession();
        return ams::Result(result);
    }
};
struct Options : ams::sf::hipc::DefaultServerManagerOptions {
    static constexpr bool CanManageMitmServers=true;
};
class Manager : public ams::sf::hipc::ServerManager<1,Options,8> {
    ams::Result OnNeedsToAccept(int,Server* server) override {
        std::shared_ptr<::Service> forward;
        ams::sm::MitmProcessInfo client{};
        server->AcknowledgeMitmSession(&forward,&client);
        return AcceptMitmImpl(server,ams::sf::CreateSharedObjectEmplaced<ams::pm_stop_sf::IPmStop,PmStop>(decltype(forward)(forward),client),forward);
    }
public:
    ams::Result install() { return RegisterMitmServer<PmStop>(0,ams::sm::ServiceName::Encode("pm:shell")); }
};
ams::os::ThreadType thread;
alignas(0x1000) std::uint8_t stack[0x8000];
void run(void*) {
    // pm:shell is needed to finish system startup. Never declare it as a
    // future MITM in mitm.lst; attach only after native HOME has entered.
    while (!launch::native_home_ready())
        ams::os::SleepThread(ams::TimeSpan::FromMilliSeconds(100));
    static Manager manager;
    const auto rc=manager.install();
    startup(120,rc.GetValue());
    if (R_SUCCEEDED(rc)) manager.LoopProcess();
}
}
void start_pm_stop() {
    const auto rc=ams::os::CreateThread(&thread,run,nullptr,stack,sizeof(stack),18,3);
    startup(119,rc.GetValue());
    if (R_SUCCEEDED(rc)) {
        ams::os::SetThreadNamePointer(&thread,"nshome-pm-stop");
        ams::os::StartThread(&thread);
    }
}
}
