// SPDX-License-Identifier: GPL-2.0-only
#include <sys/reent.h>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <limits>
extern "C" void* _malloc_r(_reent*, std::size_t);
extern "C" void _free_r(_reent*, void*);
extern "C" void* _calloc_r(_reent*, std::size_t, std::size_t);
extern "C" void* _realloc_r(_reent*, void*, std::size_t);
int main() {
    _reent r{};
    auto* p = static_cast<char*>(_malloc_r(&r, 16));
    assert(p);
    std::memcpy(p, "stdio", 6);
    p = static_cast<char*>(_realloc_r(&r, p, 64));
    assert(p && std::memcmp(p, "stdio", 6) == 0);
    assert(_realloc_r(&r, p, 0) == nullptr);
    auto* zero = static_cast<unsigned char*>(_calloc_r(&r, 8, 8));
    assert(zero);
    for (int i = 0; i < 64; ++i) assert(zero[i] == 0);
    _free_r(&r, zero);
    _free_r(&r, nullptr);
    assert(!_calloc_r(&r, std::numeric_limits<std::size_t>::max(), 2));
    assert(r._errno == ENOMEM);
    _free_r(&r, _realloc_r(&r, nullptr, 32));
}
