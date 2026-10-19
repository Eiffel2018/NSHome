// SPDX-License-Identifier: GPL-2.0-only
#include "breeze_channel.hpp"
#include <cassert>
using namespace observer::background;
int main() {
    Service creator{2},holder{3};
    Message message{Magic,EnterHidden,0,0};
    assert(!send_message(creator,holder,message));
    assert(calls.back().object==3 && calls.back().command==103);
    assert(calls.back().params.in_num_objects==1);
    Message response{Magic,Ack,EnterHidden,0};
    std::memcpy(incoming,&response,sizeof(response));
    for(unsigned failure=0;failure<=4;++failure) {
        calls.clear();closed.clear();fail_at=failure;
        Message got{};
        const auto rc=pop_message(holder,got);
        assert(rc==(failure?0x1234U:0U));
        assert(calls.front().object==3 && calls.front().command==104);
        if(!failure) {
            assert(ack(got,EnterHidden));
            const auto& read=calls.back();
            assert(read.command==11 && read.input==0);
            assert(read.params.buffer_attrs.attr==(SfBufferAttr_HipcAutoSelect|SfBufferAttr_Out));
            assert(read.params.buffers[0].size==16);
        }
        assert(closed.size()==(failure==1?0U:failure==2?1U:2U));
    }
    fail_at=0;
    for(auto size:{0,15,17,4096}) {
        calls.clear();closed.clear();storage_size=size;
        Message got{};
        assert(pop_message(holder,got));
        assert(calls.size()==3 && closed.size()==2); // No oversized Read.
    }
}
