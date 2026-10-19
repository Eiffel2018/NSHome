// SPDX-License-Identifier: GPL-2.0-only
// Adapted from SwitchU 546903f6 projects/daemon/src/ecs.cpp; uses existing AMS runtime.
#include "trace.hpp"
#include "launch_ecs.hpp"
#include <stratosphere/fssrv/interface_adapters/fssrv_filesystem_interface_adapter.hpp>
#include <mutex>

namespace observer::launch {
namespace {
constexpr std::uint64_t Album = 0x010000000000100d;
struct Options {
    static constexpr std::size_t PointerBufferSize = 0x800, MaxDomains = 0x10, MaxDomainObjects = 0x40;
    static constexpr bool CanDeferInvokeRequest = false, CanManageMitmServers = false;
};
using Manager = ams::sf::hipc::ServerManager<0, Options, 8>;
Manager& manager() {
    // Construct after InitializeSystemModule installs the shared allocator.
    static Manager instance;
    return instance;
}
ams::os::ThreadType thread;
alignas(0x1000) std::uint8_t stack[0x8000];
bool started{}, registered{}, loader_service{};
void run(void* server) { static_cast<Manager*>(server)->LoopProcess(); }
}
std::uint32_t unregister_loader() {
    if (!registered) return 0;
    const auto rc = serviceDispatchIn(ldrShellGetServiceSession(), 65001, Album);
    if (R_SUCCEEDED(rc)) registered = false;
    return rc;
}
std::uint32_t register_loader() {
    if (registered) return MAKERESULT(Module_Libnx, LibnxError_AlreadyInitialized);
    if (!started) {
        const auto rc = ams::os::CreateThread(&thread, run, &manager(), stack, sizeof(stack), 16, 3);
        if (rc.IsFailure()) return rc.GetValue();
        ams::os::SetThreadNamePointer(&thread, "nshome-loader-fs");
        ams::os::StartThread(&thread); started = true;
    }
    auto rc = loader_service ? 0 : ldrShellInitialize();
    if (R_FAILED(rc)) return rc;
    loader_service = true;
    Handle handle = INVALID_HANDLE;
    rc = serviceDispatchIn(ldrShellGetServiceSession(), 65000, Album,
        .out_handle_attrs = { SfOutHandleAttr_HipcMove }, .out_handles = &handle);
    if (R_FAILED(rc)) return rc;
    registered = true;
    FsFileSystem sd{};
    rc = fsOpenSdCardFileSystem(&sd);
    if (R_SUCCEEDED(rc)) {
        auto remote = std::make_shared<ams::fs::RemoteFileSystem>(sd);
        auto directory = std::make_shared<ams::fssystem::SubDirectoryFileSystem>(std::move(remote));
        ams::fs::Path path;
        auto result = path.Initialize("/atmosphere/contents/00FF4E5348414D01/breeze_loader");
        if (result.IsSuccess()) result = path.Normalize(ams::fs::PathFlags{});
        if (result.IsSuccess()) result = directory->Initialize(path);
        if (result.IsSuccess()) {
            auto object = ams::sf::CreateSharedObjectEmplaced<ams::fssrv::sf::IFileSystem,
                ams::fssrv::impl::FileSystemInterfaceAdapter>(std::move(directory), false);
            result = manager().RegisterSession(handle, ams::sf::cmif::ServiceObjectHolder(std::move(object)));
        }
        rc = result.GetValue();
    }
    if (R_FAILED(rc)) { (void)unregister_loader(); svcCloseHandle(handle); }
    return rc;
}
}
