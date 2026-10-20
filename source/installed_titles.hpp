// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace nshome::titles {
constexpr unsigned PageMax = 16, EntriesMax = 4096;
struct Entry {
    std::uint64_t id{};
    std::string name, name_source = "unavailable";
    std::uint32_t name_result{};
    unsigned storage{};
};
// Backend errors abort publication; a missing name never hides a verified title.
template<class Backend>
std::uint32_t collect(Backend& backend, std::vector<Entry>& rows, unsigned& scanned) {
    std::vector<std::uint64_t> ids;
    auto rc=backend.records(ids);
    if (rc) return rc;
    std::sort(ids.begin(),ids.end()); ids.erase(std::unique(ids.begin(),ids.end()),ids.end());
    if (ids.size()>EntriesMax) return 1;
    scanned=ids.size();
    for (const auto id : ids) {
        bool installed=false, complete=false; Entry row; row.id=id;
        rc=backend.installed(id,installed);
        if (!rc && installed) rc=backend.base(id,row.storage,complete);
        if (rc) return rc;
        if (!installed || !complete) continue;
        backend.name(row); rows.push_back(std::move(row));
    }
    return backend.changed();
}
inline std::string quote(std::string_view value) {
    std::string out = "\"";
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') { out += '\\'; out += c; }
        else if (c < 32) { char bytes[7]; std::snprintf(bytes,sizeof(bytes),"\\u%04x",c); out += bytes; }
        else out += c;
    }
    return out + '"';
}
// Never emit malformed UTF-8 or cut through a multibyte name at the wire bound.
inline std::string name_utf8(std::string_view value) {
    std::string out;
    for (std::size_t i=0; i<value.size() && out.size()<511;) {
        const auto c = static_cast<unsigned char>(value[i]);
        unsigned length = c<128 ? 1 : c>=0xc2 && c<=0xdf ? 2 : c>=0xe0 && c<=0xef ? 3 : c>=0xf0 && c<=0xf4 ? 4 : 0;
        bool valid = length && i+length<=value.size();
        for (unsigned j=1; valid && j<length; ++j) valid = (static_cast<unsigned char>(value[i+j]) & 0xc0)==0x80;
        if (valid && length>=3) {
            const auto next=static_cast<unsigned char>(value[i+1]);
            valid = !(c==0xe0 && next<0xa0) && !(c==0xed && next>=0xa0) && !(c==0xf0 && next<0x90) && !(c==0xf4 && next>=0x90);
        }
        const std::string_view part = valid ? value.substr(i,length) : std::string_view("\xef\xbf\xbd");
        if (out.size()+part.size()>511) break;
        out += part; i += valid ? length : 1;
    }
    return out;
}
// Caller serializes access; native IPC and catalogue construction run outside its lock.
class Catalogue {
public:
    std::uint64_t boot{}, generation{};
    std::string state = "idle";
    std::uint32_t result{};
    unsigned scanned{};
    std::vector<Entry> entries;
    bool begin() {
        if (state=="building") return false;
        ++generation; state="building"; result=0; scanned=0; entries.clear(); return true;
    }
    void finish(std::uint64_t ticket, std::vector<Entry> rows, unsigned count, std::uint32_t rc) {
        if (ticket!=generation || state!="building") return;
        scanned=count; result=rc;
        if (rc || rows.size()>EntriesMax) { state="failed"; if (!result) result=1; return; }
        std::sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){return a.id<b.id;});
        rows.erase(std::unique(rows.begin(),rows.end(),[](const auto& a,const auto& b){return a.id==b.id;}),rows.end());
        entries=std::move(rows); state="ready";
    }
    void invalidate() { ++generation; entries.clear(); state="invalidated"; }
    std::string header() const {
        char rc[16]; std::snprintf(rc,sizeof(rc),"0x%08X",result);
        return "{\"schema\":1,\"source\":\"ns+ncm.full_application_contents\",\"boot\":"+quote(std::to_string(boot))+
            ",\"generation\":"+quote(std::to_string(generation))+",\"state\":"+quote(state)+
            ",\"rc\":"+quote(rc)+",\"total\":"+std::to_string(entries.size())+",\"scanned\":"+std::to_string(scanned)+
            ",\"page_max\":16,\"profile_selection_supported\":false";
    }
    std::string status() const { return "OK titles "+header()+"}\n"; }
    std::string page(std::uint64_t expected_boot, std::uint64_t expected_generation, unsigned offset, unsigned limit) const {
        if (expected_boot!=boot || expected_generation!=generation) return "ERR titles-stale\n";
        if (state!="ready") return "ERR titles-not-ready\n";
        if (!limit || limit>PageMax || offset>entries.size()) return "ERR titles-range\n";
        const auto end=std::min(entries.size(),std::size_t(offset)+limit);
        std::string out="OK titles "+header()+",\"offset\":"+std::to_string(offset)+",\"next\":"+
            (end<entries.size() ? std::to_string(end) : "null")+",\"items\":[";
        for (std::size_t i=offset;i<end;++i) {
            const auto& row=entries[i]; char id[17], rc[16];
            std::snprintf(id,sizeof(id),"%016llX",static_cast<unsigned long long>(row.id));
            std::snprintf(rc,sizeof(rc),"0x%08X",row.name_result);
            if (i!=offset) out+=',';
            out+="{\"title_id\":"+quote(id)+",\"name\":"+(row.name.empty()?"null":quote(row.name))+
                ",\"name_source\":"+quote(row.name_source)+",\"name_rc\":"+quote(rc)+
                ",\"storage_id\":"+std::to_string(row.storage)+"}";
        }
        return out+"]}\n";
    }
};
}
