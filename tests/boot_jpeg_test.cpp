// SPDX-License-Identifier: GPL-2.0-only
#include "boot_screen.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <vector>
int main(int argc, char** argv) {
    assert(!observer::boot_jpeg_ready(nullptr, 0));
    const unsigned char truncated[] = {0xff,0xd8,0,0};
    assert(!observer::boot_jpeg_ready(truncated, sizeof(truncated)));
    assert(!observer::boot_jpeg_ready(truncated, 0x80001));
    for (int i=1; i<argc; ++i) {
        std::ifstream file(argv[i], std::ios::binary);
        assert(file);
        const std::vector<char> jpeg((std::istreambuf_iterator<char>(file)), {});
        assert(observer::boot_jpeg_ready(jpeg.data(), jpeg.size()) == (i == 1));
    }
}
