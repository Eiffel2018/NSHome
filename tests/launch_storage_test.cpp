// SPDX-License-Identifier: GPL-2.0-only
#include "launch_storage.hpp"
#include <cassert>
int main() {
    Service creator{2},holder{3};
    const unsigned char data[32]={};
    for(unsigned failure=0;failure<=4;++failure){
        calls.clear();closed.clear();fail_at=failure;
        const auto rc=observer::launch::push_storage(&creator,&holder,100,data,sizeof(data));
        assert(rc==(failure?0x1234U:0U));
        assert(calls[0].object==2&&calls[0].command==10&&calls[0].input==32);
        if(!failure||failure>=3){
            const auto& write=calls[2];
            assert(write.command==10&&write.input==0);
            // Protocol requirement: auto-select, not a plain map-alias buffer.
            assert(write.params.buffer_attrs.attr==(SfBufferAttr_HipcAutoSelect|SfBufferAttr_In));
            assert(write.params.buffers[0].data==data&&write.params.buffers[0].size==32);
        }
        if(!failure||failure>=4){assert(calls[3].object==3&&calls[3].command==100);assert(calls[3].params.in_num_objects==1);}
        assert(closed.size()==(failure==1?0U:failure==2?1U:2U));
    }
}
