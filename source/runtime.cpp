// SPDX-License-Identifier: GPL-2.0-only
#include <cstdlib>
#include <cstddef>
#include <cerrno>
#include <limits>
#include <sys/reent.h>

// Stratosphere owns malloc and intentionally supplies no newlib sbrk heap.
// Newlib stdio calls these reentrant entry points directly. Route every
// allocation/free pair to the same allocator as C++ and libnx devoptab.
extern "C" void* _malloc_r(struct _reent* r, std::size_t size) {
    void* p = std::malloc(size);
    if (!p && size) r->_errno = ENOMEM;
    return p;
}
extern "C" void _free_r(struct _reent*, void* p) { std::free(p); }
extern "C" void* _calloc_r(struct _reent* r, std::size_t count, std::size_t size) {
    if (size && count > std::numeric_limits<std::size_t>::max() / size) {
        r->_errno = ENOMEM;
        return nullptr;
    }
    void* p = std::calloc(count, size);
    if (!p && count && size) r->_errno = ENOMEM;
    return p;
}
extern "C" void* _realloc_r(struct _reent* r, void* p, std::size_t size) {
    if (!size) { std::free(p); return nullptr; }
    void* result = p ? std::realloc(p, size) : std::malloc(size);
    if (!result) r->_errno = ENOMEM;
    return result;
}
// Newlib's stdio may request aligned buffers through this entry point. Its
// default implementation edits malloc chunk headers, which do not exist in
// Stratosphere's allocator. Never let it operate on our _malloc_r pointers.
extern "C" void* _memalign_r(struct _reent* r, std::size_t alignment, std::size_t size) {
    if (!alignment || (alignment & (alignment - 1))) {
        r->_errno = EINVAL;
        return nullptr;
    }
    if (alignment <= alignof(std::max_align_t)) return _malloc_r(r, size ? size : 1);
    if (size > std::numeric_limits<std::size_t>::max() - (alignment - 1)) {
        r->_errno = ENOMEM;
        return nullptr;
    }
    const auto padded = size ? (size + alignment - 1) & ~(alignment - 1) : alignment;
    void* result = std::aligned_alloc(alignment, padded);
    if (!result) r->_errno = ENOMEM;
    return result;
}
