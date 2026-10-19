// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "breeze_background.hpp"
#include "launch_storage.hpp"
namespace observer::background {
inline Result send_message(Service& creator, Service& holder, const Message& message) {
    // libnx 20cbcfd9 appletHolderPushInteractiveInData (102 is ExtraStorage).
    return launch::push_storage(&creator, &holder, 103, &message, sizeof(message));
}
inline Result pop_message(Service& holder, Message& message) {
    Service storage{}, accessor{};
    // libnx 20cbcfd9 appletHolderPopInteractiveOutData.
    auto rc = launch::child(&holder, 104, &storage);
    if (R_SUCCEEDED(rc)) rc = launch::child(&storage, 0, &accessor);
    s64 size = 0;
    if (R_SUCCEEDED(rc)) rc = serviceDispatchOut(&accessor, 0, size);
    if (R_SUCCEEDED(rc) && size != sizeof(message)) rc = MAKERESULT(Module_Libnx, LibnxError_BadInput);
    const s64 offset = 0;
    if (R_SUCCEEDED(rc)) rc = serviceDispatchIn(&accessor, 11, offset,
        .buffer_attrs = { SfBufferAttr_HipcAutoSelect | SfBufferAttr_Out }, .buffers = { { &message, sizeof(message) } });
    if (serviceIsActive(&accessor)) serviceClose(&accessor);
    if (serviceIsActive(&storage)) serviceClose(&storage);
    return rc;
}
}
