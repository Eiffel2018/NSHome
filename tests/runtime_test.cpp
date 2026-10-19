// SPDX-License-Identifier: GPL-2.0-only
#include <sys/reent.h>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <limits>
#include <cstdint>
#include <initializer_list>
extern "C" void* _malloc_r(_reent*, std::size_t);
extern "C" void _free_r(_reent*, void*);
extern "C" void* _calloc_r(_reent*, std::size_t, std::size_t);
extern "C" void* _realloc_r(_reent*, void*, std::size_t);
extern "C" void* _memalign_r(_reent*, std::size_t, std::size_t);
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
    for (const std::size_t alignment : {std::size_t(1), std::size_t(16), std::size_t(4096)}) {
        for (const std::size_t size : {std::size_t(0), std::size_t(1), std::size_t(560), std::size_t(4096)}) {
            auto* aligned = static_cast<unsigned char*>(_memalign_r(&r, alignment, size));
            assert(aligned && reinterpret_cast<std::uintptr_t>(aligned) % alignment == 0);
            if (size) std::memset(aligned, 0xa5, size);
            // Same allocator owns both aligned stdio buffers and ordinary C++ allocations.
            _free_r(&r, aligned);
        }
    }
    assert(!_memalign_r(&r, 0, 560) && r._errno == EINVAL);
    assert(!_memalign_r(&r, 3, 560) && r._errno == EINVAL);
    assert(!_memalign_r(&r, 4096, std::numeric_limits<std::size_t>::max()) && r._errno == ENOMEM);
}
