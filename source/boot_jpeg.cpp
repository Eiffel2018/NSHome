// SPDX-License-Identifier: GPL-2.0-only
#include "boot_screen.hpp"
#include "close_screen.hpp"
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_SIMD
#define STBI_MAX_DIMENSIONS 1280
#define STB_IMAGE_IMPLEMENTATION
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "stb_image.h"
#pragma GCC diagnostic pop
namespace observer {
static bool jpeg_ready(const void* bytes, std::size_t size, bool close, bool confirm) {
    if (!bytes || size < 4 || size > 0x80000) return false;
    const auto* jpeg = static_cast<const unsigned char*>(bytes);
    int width = 0, height = 0, channels = 0;
    if (jpeg[0] != 0xff || jpeg[1] != 0xd8 ||
        !stbi_info_from_memory(jpeg, int(size), &width, &height, &channels) || width != 1280 || height != 720) return false;
    auto* gray = stbi_load_from_memory(jpeg, int(size), &width, &height, &channels, 1);
    const bool ready = gray && (close ? stock_close_image(gray,width,height,confirm) : boot_prompt_image(gray,width,height));
    stbi_image_free(gray);
    return ready;
}
bool boot_jpeg_ready(const void* bytes, std::size_t size) { return jpeg_ready(bytes,size,false,false); }
bool stock_close_jpeg(const void* bytes, std::size_t size, bool confirm) { return jpeg_ready(bytes,size,true,confirm); }
}
