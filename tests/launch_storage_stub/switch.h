// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <cstring>
using u32 = std::uint32_t;
using s64 = std::int64_t;
using Result = std::uint32_t;
struct Service { u32 object_id{}; bool own_handle{}; };
constexpr u32 SfBufferAttr_In = 1, SfBufferAttr_Out = 2, SfBufferAttr_HipcMapAlias = 4, SfBufferAttr_HipcAutoSelect = 32;
constexpr u32 Module_Libnx=345, LibnxError_BadInput=11;
#define MAKERESULT(m,d) (((d)<<9)|(m))
struct Dispatch {
    struct { u32 attr{}; } buffer_attrs{};
    struct Buffer { const void* data; std::size_t size; } buffers[1]{};
    u32 in_num_objects{}; const Service* in_objects[1]{};
    u32 out_num_objects{}; Service* out_objects{};
};
struct Call {u32 object, command; Dispatch params; s64 input;};
inline std::vector<Call> calls;
inline std::vector<u32> closed;
inline unsigned fail_at{};
inline s64 storage_size=16;
inline unsigned char incoming[16]{};
inline Result dispatch(Service* s,u32 command,const void* input,Dispatch params) {
    s64 value = input ? *static_cast<const s64*>(input) : 0;
    calls.push_back({s->object_id,command,params,value});
    if(calls.size()==fail_at)return 0x1234;
    if(params.out_objects)params.out_objects->object_id=10+static_cast<u32>(calls.size());
    if(command==11 && params.buffers[0].data && (params.buffer_attrs.attr&SfBufferAttr_Out))
        std::memcpy(const_cast<void*>(params.buffers[0].data),incoming,params.buffers[0].size);
    return 0;
}
inline Result dispatch_out(Service* s,u32 command,s64* out) {
    const auto rc=dispatch(s,command,nullptr,{});
    if(!rc)*out=storage_size;
    return rc;
}
inline bool serviceIsActive(Service* s){return s->object_id;}
inline void serviceClose(Service* s){closed.push_back(s->object_id);*s={};}
#define R_SUCCEEDED(r) ((r)==0)
#define serviceDispatch(s,c,...) dispatch(s,c,nullptr,Dispatch{__VA_ARGS__})
#define serviceDispatchIn(s,c,i,...) dispatch(s,c,&(i),Dispatch{__VA_ARGS__})
#define serviceDispatchOut(s,c,o,...) dispatch_out(s,c,&(o))
