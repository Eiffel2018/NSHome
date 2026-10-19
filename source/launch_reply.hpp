// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "codec.hpp"
namespace observer::launch {
// Header-only CMIF reply with no handles/objects, for one replaced command.
// The caller has already validated a domain request and restored its TLS.
inline void make_reply(void* output, bool domain, std::uint32_t result, std::uint32_t token) {
    auto* bytes = static_cast<std::uint8_t*>(output);
    std::memset(bytes, 0, 0x100);
    const std::uint32_t words = domain ? 12 : 8;
    std::memcpy(bytes + 4, &words, 4); // HIPC data-word count; no descriptors.
    auto* payload = bytes + 16 + (domain ? 16 : 0);
    std::memcpy(payload, &OutMagic, 4);
    std::memcpy(payload + 8, &result, 4);
    std::memcpy(payload + 12, &token, 4);
}
}
