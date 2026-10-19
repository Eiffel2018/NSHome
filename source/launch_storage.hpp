// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <switch.h>
namespace observer::launch {
inline Service borrowed(const Service& connection, std::uint32_t object) {
    Service result = connection; result.object_id = object; result.own_handle = false; return result;
}
inline Result child(Service* parent, std::uint32_t command, Service* out) {
    return serviceDispatch(parent, command, .out_num_objects = 1, .out_objects = out);
}
inline Result push_storage(Service* creator, Service* destination, std::uint32_t command,
                           const void* data, std::size_t size) {
    Service storage{}, accessor{};
    const s64 length = size;
    auto rc = serviceDispatchIn(creator, 10, length, .out_num_objects = 1, .out_objects = &storage);
    if (R_SUCCEEDED(rc)) rc = child(&storage, 0, &accessor);
    const s64 offset = 0;
    if (R_SUCCEEDED(rc)) rc = serviceDispatchIn(&accessor, 10, offset,
        // IStorageAccessor::Write uses an auto-select buffer (libnx's
        // _appletStorageAccessorRW), including its pointer descriptor. A plain
        // map-alias buffer is a malformed request and closes the AM session.
        .buffer_attrs = { SfBufferAttr_HipcAutoSelect | SfBufferAttr_In }, .buffers = { {data, size} });
    if (serviceIsActive(&accessor)) serviceClose(&accessor);
    if (R_SUCCEEDED(rc)) rc = serviceDispatch(destination, command, .in_num_objects = 1, .in_objects = { &storage });
    if (serviceIsActive(&storage)) serviceClose(&storage);
    return rc;
}
}
