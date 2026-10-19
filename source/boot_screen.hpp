// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstddef>
#include <cstdint>
namespace observer {
// Geometry check for the observed stock boot screen, combined with AM guards.
bool boot_screen_ready();
inline bool boot_prompt_image(const std::uint8_t* gray, unsigned width, unsigned height) {
    if (!gray || width != 1280 || height != 720) return false;
    const int background = gray[500 * width + 820];
    auto ink = [&](unsigned x, unsigned y) {
        int contrast = int(gray[y * width + x]) - background;
        return contrast >= 100 || contrast <= -100;
    };
    // Observed stock boot screen: large circled HOME symbol, house doorway,
    // and the separate A-start hint. No Nintendo bitmap is embedded.
    if (!ink(820,265) || !ink(820,429) || !ink(738,346) || !ink(902,346) ||
        !ink(820,310) || !ink(794,362) || ink(820,350) || ink(820,245)) return false;
    for (unsigned x=730; x<=850; x+=2)
        for (unsigned y=642; y<=650; y+=2)
            if (ink(x,y-20) && ink(x,y+20) && ink(x-21,y) && ink(x+21,y)) return true;
    return false;
}
bool boot_jpeg_ready(const void* bytes, std::size_t size);
}
