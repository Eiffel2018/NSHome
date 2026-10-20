// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstddef>
#include <cstdint>
#include <initializer_list>
namespace observer {
// Stock 1280x720 HOME only. Unknown layouts fail closed, without confirming.
inline bool stock_close_image(const std::uint8_t* g, unsigned w, unsigned h, bool confirm) {
    if (!g || w != 1280 || h != 720) return false;
    const auto at = [&](unsigned x, unsigned y) { return int(g[y*w+x]); };
    const auto distance = [](int a, int b) { return a>b ? a-b : b-a; };
    if (!confirm) {
        // The stock X/Close hint is present only for a selected running title.
        const auto center = at(985,685);
        for (auto x : {975u,995u}) if (distance(at(x,685),center)<100) return false;
        for (auto y : {675u,695u}) if (distance(at(985,y),center)<100) return false;
        return true;
    }
    // Native Close dialog: uniform panel, dimmed outside, right Close selected.
    // No font, language or Nintendo bitmap is embedded.
    const auto panel = at(970,250);
    for (auto x : {600u,970u})
        for (auto y : {250u,400u}) if (distance(at(x,y),panel)>15) return false;
    if (distance(at(310,440),panel)>15 || distance(at(1000,440),panel)>15 ||
        distance(at(1100,300),panel)<30 || distance(at(450,458),panel)>20) return false;
    for (auto x : {700u,900u}) if (distance(at(x,458),panel)<50) return false;
    for (auto x : {638u,1028u}) if (distance(at(x,490),panel)<50) return false;
    int low=255, high=0;
    for (auto x : {340u,400u,460u}) for (auto y : {260u,320u,380u}) {
        const int pixel=at(x,y); if (pixel<low) low=pixel; if (pixel>high) high=pixel;
    }
    return high-low>=25; // A title icon, not an empty generic panel.
}
bool stock_close_jpeg(const void* bytes, std::size_t size, bool confirm);
bool stock_close_screen(bool confirm);
}
