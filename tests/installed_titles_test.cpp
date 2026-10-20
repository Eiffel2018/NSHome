// SPDX-License-Identifier: GPL-2.0-only
#include "installed_titles.hpp"
#include <cassert>
using namespace nshome::titles;
struct Backend {
    bool fail{}, changed_during_read{};
    unsigned records(std::vector<std::uint64_t>& out) { out={4,3,2,1,1}; return 0; }
    unsigned installed(std::uint64_t id,bool& out) { out=id!=1; return fail && id==3 ? 123 : 0; }
    unsigned base(std::uint64_t id,unsigned& storage,bool& out) { out=id!=2; storage=5; return 0; }
    void name(Entry& out) { if (out.id==3) { out.name="遊戲\n\"one\""; out.name_source="nacp.system_language"; } else out.name_result=456; }
    unsigned changed() { return changed_during_read ? 789 : 0; }
};
int main() {
    Catalogue c; c.boot=9007199254740993ULL;
    assert(c.begin() && !c.begin());
    Backend b; std::vector<Entry> rows; unsigned scanned=0;
    assert(!collect(b,rows,scanned) && scanned==4 && rows.size()==2);
    c.finish(1,std::move(rows),scanned,0);
    assert(c.state=="ready" && c.entries[0].id==3 && c.entries[1].id==4);
    const auto first=c.page(c.boot,1,0,1);
    assert(first.find("9007199254740993")!=std::string::npos);
    assert(first.find("遊戲\\u000a\\\"one\\\"")!=std::string::npos && first.find("\"next\":1")!=std::string::npos);
    const auto last=c.page(c.boot,1,1,16);
    assert(last.find("\"name\":null")!=std::string::npos && last.find("\"next\":null")!=std::string::npos);
    assert(c.page(c.boot,1,3,1)=="ERR titles-range\n");
    assert(c.page(c.boot,1,0,0)=="ERR titles-range\n");
    assert(c.page(c.boot+1,1,0,1)=="ERR titles-stale\n");
    c.invalidate(); assert(c.entries.empty() && c.page(c.boot,1,0,1)=="ERR titles-stale\n");
    assert(c.begin()); b.fail=true; rows.clear();
    auto rc=collect(b,rows,scanned); assert(rc==123);
    c.finish(c.generation,std::move(rows),scanned,rc);
    assert(c.state=="failed" && c.entries.empty()); // No partial catalogue on provider error.
    assert(c.begin()); rows.clear(); b.fail=false; b.changed_during_read=true;
    rc=collect(b,rows,scanned); assert(rc==789); c.finish(c.generation,std::move(rows),scanned,rc);
    assert(c.state=="failed" && c.entries.empty());
    c.begin(); auto generation=c.generation;
    c.finish(generation-1,{},0,0); assert(c.state=="building");
    c.finish(generation,{},0,0); assert(c.state=="ready" && c.page(c.boot,generation,0,16).find("\"items\":[]")!=std::string::npos);
    assert(name_utf8("\xc0\xaf")=="\xef\xbf\xbd\xef\xbf\xbd");
    assert(name_utf8("\xed\xa0\x80")==std::string("\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd"));
    assert(name_utf8(std::string(510,'a')+"遊").size()==510);
}
