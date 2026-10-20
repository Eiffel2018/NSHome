// SPDX-License-Identifier: GPL-2.0-only
#include "trace.hpp"
#include "boot_screen.hpp"
#include "close_screen.hpp"
#include "remote_nro_capture.hpp"
#include <cstdlib>
namespace observer {
bool stock_close_screen(bool confirm) {
    void* jpeg = nullptr;
    std::size_t size = 0;
    const auto rc = nshome::remote_capture::captureJpeg(&jpeg,&size,ViLayerStack_Default);
    const bool ready = R_SUCCEEDED(rc) && stock_close_jpeg(jpeg,size,confirm);
    std::free(jpeg);
    return ready;
}
bool boot_screen_ready() {
    void* jpeg = nullptr;
    std::size_t size = 0;
    const auto rc = nshome::remote_capture::captureJpeg(&jpeg, &size);
    const bool ready = R_SUCCEEDED(rc) && boot_jpeg_ready(jpeg, size);
    std::free(jpeg);
    startup(33, rc, ready);
    return R_SUCCEEDED(rc) && ready;
}
}
