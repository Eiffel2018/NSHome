// SPDX-License-Identifier: GPL-2.0-only
#include "remote_nro_titles.hpp"
#include "installed_titles.hpp"
#include <switch.h>
#include <stratosphere.hpp>
#include <array>
#include <memory>
#include <mutex>
#include <cstring>

namespace nshome::titles {
namespace {
constexpr Result Invalid=MAKERESULT(Module_Libnx,LibnxError_BadInput);
std::mutex mutex;
Catalogue catalogue;
bool started{}, pending{};
ams::os::ThreadType thread;
alignas(0x1000) std::uint8_t stack[0x10000];

// These are all read-only NS/NCM calls. A record alone is NOT installation proof.
struct Backend {
    Event records_event{}, sd_event{};
    bool ready{}, ncm_ready{};
    std::uint64_t deadline{};
    Result budget() const { return armTicksToNs(armGetSystemTick())/1000000>=deadline ? KERNELRESULT(TimedOut) : 0; }
    Result open() {
        if (ready) return 0;
        auto rc=nsInitialize();
        if (R_FAILED(rc)) return rc;
        rc=nsGetApplicationRecordUpdateSystemEvent(&records_event);
        if (R_SUCCEEDED(rc)) rc=nsGetSdCardMountStatusChangedEvent(&sd_event);
        if (R_FAILED(rc)) {
            if (records_event.revent) eventClose(&records_event);
            nsExit(); return rc;
        }
        ready=true; return 0;
    }
    Result changed() {
        if (R_FAILED(budget())) return KERNELRESULT(TimedOut);
        return ready && (R_SUCCEEDED(eventWait(&records_event,0)) || R_SUCCEEDED(eventWait(&sd_event,0))) ? Invalid : 0;
    }
    void clear_events() { eventClear(&records_event); eventClear(&sd_event); }
    Result records(std::vector<std::uint64_t>& ids) {
        std::array<NsApplicationRecord,64> batch;
        for (s32 offset=0;;) {
            if (R_FAILED(budget())) return KERNELRESULT(TimedOut);
            s32 count=0; auto rc=nsListApplicationRecord(batch.data(),batch.size(),offset,&count);
            if (R_FAILED(rc)) return rc;
            if (count<0 || count>static_cast<s32>(batch.size()) || ids.size()+count>EntriesMax) return Invalid;
            if (!count) return 0;
            for (s32 i=0;i<count;++i) { if (!batch[i].application_id) return Invalid; ids.push_back(batch[i].application_id); }
            offset+=count;
        }
    }
    Result installed(std::uint64_t id, bool& value) {
        if (R_FAILED(budget())) return KERNELRESULT(TimedOut);
        return nsIsAnyApplicationEntityInstalled(id,&value);
    }
    Result full_base(std::uint64_t id, NcmStorageId storage, bool& present) {
        NcmContentMetaDatabase db{}; NcmContentStorage content{};
        auto rc=ncmOpenContentMetaDatabase(&db,storage);
        if (R_SUCCEEDED(rc)) rc=ncmOpenContentStorage(&content,storage);
        NcmContentMetaKey key{};
        if (R_SUCCEEDED(rc)) rc=ncmContentMetaDatabaseGetLatestContentMetaKey(&db,&key,id);
        if (R_SUCCEEDED(rc) && key.id==id && key.type==NcmContentMetaType_Application && key.install_type==NcmContentInstallType_Full) {
            std::array<NcmContentInfo,32> infos; s32 offset=0; bool all=true;
            for (;;) {
                if (R_FAILED(budget())) { rc=KERNELRESULT(TimedOut); break; }
                s32 count=0; rc=ncmContentMetaDatabaseListContentInfo(&db,&count,infos.data(),infos.size(),&key,offset);
                if (R_FAILED(rc) || count<0 || count>static_cast<s32>(infos.size())) { if (R_SUCCEEDED(rc)) rc=Invalid; break; }
                if (!count) { present=all && offset>0; break; }
                for (s32 i=0;i<count;++i) {
                    bool exists=false; rc=ncmContentStorageHas(&content,&exists,&infos[i].content_id);
                    if (R_FAILED(rc)) break;
                    if (!exists) all=false;
                }
                if (R_FAILED(rc) || !all) break;
                offset+=count;
                if (offset>65535) { rc=Invalid; break; }
            }
        }
        if (serviceIsActive(&content.s)) ncmContentStorageClose(&content);
        if (serviceIsActive(&db.s)) ncmContentMetaDatabaseClose(&db);
        return rc;
    }
    Result base(std::uint64_t id, unsigned& storage, bool& complete) {
        std::array<NsApplicationContentMetaStatus,32> batch;
        for (s32 offset=0;;) {
            if (R_FAILED(budget())) return KERNELRESULT(TimedOut);
            s32 count=0; auto rc=nsListApplicationContentMetaStatus(id,offset,batch.data(),batch.size(),&count);
            if (R_FAILED(rc)) return rc;
            if (count<0 || count>static_cast<s32>(batch.size()) || offset+count>4096) return Invalid;
            if (!count) return 0;
            for (s32 i=0;i<count;++i) {
                const auto& item=batch[i];
                if (item.meta_type!=NcmContentMetaType_Application || item.application_id!=id) continue;
                if (item.storageID!=NcmStorageId_GameCard && item.storageID!=NcmStorageId_BuiltInUser && item.storageID!=NcmStorageId_SdCard) return Invalid;
                rc=full_base(id,static_cast<NcmStorageId>(item.storageID),complete);
                if (R_FAILED(rc)) return rc;
                if (complete) { storage=item.storageID; return 0; }
            }
            offset+=count;
        }
    }
    void name(Entry& row) {
        auto data=std::make_unique<NsApplicationControlData>(); u64 size=0;
        row.name_result=nsGetApplicationControlData(NsApplicationControlSource_Storage,row.id,data.get(),sizeof(*data),&size);
        if (row.name_result) return;
        if (size<sizeof(NacpStruct)) { row.name_result=Invalid; return; }
        NacpLanguageEntry* language=nullptr;
        const bool preferred=R_SUCCEEDED(nacpGetLanguageEntry(&data->nacp,&language)) && language && language->name[0];
        if (!preferred && data->nacp.titles_data_format==0) for (auto& item : data->nacp.lang_data.lang) if (item.name[0]) { language=&item; break; }
        if (!language || !language->name[0]) { row.name_result=Invalid; return; }
        row.name=name_utf8(std::string_view(language->name,strnlen(language->name,sizeof(language->name))));
        row.name_source=preferred ? "nacp.system_language" : "nacp.first_available";
    }
};
void worker(void*) {
    Backend backend;
    for (;;) {
        std::uint64_t ticket=0;
        { std::scoped_lock lock(mutex); if (pending) { pending=false; ticket=catalogue.generation; } }
        if (ticket) {
            backend.deadline=armTicksToNs(armGetSystemTick())/1000000+60000;
            auto rc=backend.open(); std::vector<Entry> rows; unsigned scanned=0;
            if (R_SUCCEEDED(rc)) { backend.clear_events(); rc=ncmInitialize(); backend.ncm_ready=R_SUCCEEDED(rc); }
            if (R_SUCCEEDED(rc)) rc=collect(backend,rows,scanned);
            if (backend.ncm_ready) { ncmExit(); backend.ncm_ready=false; }
            std::scoped_lock lock(mutex); catalogue.finish(ticket,std::move(rows),scanned,rc);
        } else if (backend.ready && (R_SUCCEEDED(eventWait(&backend.records_event,0)) || R_SUCCEEDED(eventWait(&backend.sd_event,0)))) {
            std::scoped_lock lock(mutex); if (catalogue.state=="ready") catalogue.invalidate();
        }
        ams::os::SleepThread(ams::TimeSpan::FromMilliSeconds(100));
    }
}
}
bool start(std::uint64_t boot) {
    std::scoped_lock lock(mutex);
    if (started) return true;
    catalogue.boot=boot;
    const auto rc=ams::os::CreateThread(&thread,worker,nullptr,stack,sizeof(stack),30,3);
    if (rc.IsFailure()) return false;
    ams::os::SetThreadNamePointer(&thread,"nshome-titles"); started=true; ams::os::StartThread(&thread); return true;
}
std::string command(const remote::Command& request) {
    std::scoped_lock lock(mutex);
    if (!started) return "ERR titles-unavailable\n";
    if (request.kind==remote::CommandKind::TitlesRefresh) { if (catalogue.begin()) pending=true; return catalogue.status(); }
    if (request.kind==remote::CommandKind::TitlesStatus) return catalogue.status();
    return catalogue.page(request.numbers[0],request.numbers[1],request.numbers[2],request.numbers[3]);
}
}
