/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define NSH_LOADER_TICKET "sdmc:/config/NSHome/breeze-ticket.bin"
#define NSH_LOADER_RECEIPT "sdmc:/config/NSHome/breeze-loader.bin"
#define NSH_BREEZE_PATH "sdmc:/switch/Breeze/Breeze.nro"
typedef struct {
    uint32_t magic, version;
    uint64_t ticket;
    char path[512];
} NshLoaderTicket;
static inline int nshLoaderTicketValid(const NshLoaderTicket *value) {
    if (value->magic != 0x5448534e || value->version != 2 || !value->ticket) return 0;
    const char *end = (const char *)memchr(value->path, 0, sizeof(value->path));
    if (!end) return 0;
    const size_t size = (size_t)(end - value->path);
    if (size < 10 || memcmp(value->path, "sdmc:/", 6)) return 0;
    if (value->path[size-4] != '.' || (value->path[size-3] | 32) != 'n' ||
        (value->path[size-2] | 32) != 'r' || (value->path[size-1] | 32) != 'o') return 0;
    size_t start = 6;
    for (size_t i=6; i<=size; ++i) {
        const unsigned char c = (unsigned char)value->path[i];
        if (i<size && (c<32 || c==127 || c=='\\' || c==':')) return 0;
        if (!c || c=='/') {
            const size_t count=i-start;
            if (!count || (count==1 && value->path[start]=='.') ||
                (count==2 && value->path[start]=='.' && value->path[start+1]=='.')) return 0;
            start=i+1;
        }
    }
    return 1;
}
static inline int nshLoaderTicketDecode(const void *data, size_t size, NshLoaderTicket *out) {
    if (size == sizeof(uint64_t)) {
        uint32_t prefix[2]; memcpy(prefix, data, sizeof(prefix));
        if (prefix[0] == 0x5448534e && prefix[1] == 2) return 0;
        memset(out, 0, sizeof(*out));
        out->magic = 0x5448534e; out->version = 2;
        memcpy(&out->ticket, data, sizeof(out->ticket));
        memcpy(out->path, NSH_BREEZE_PATH, sizeof(NSH_BREEZE_PATH));
        return nshLoaderTicketValid(out);
    }
    if (size != sizeof(*out)) return 0;
    memcpy(out, data, sizeof(*out));
    return nshLoaderTicketValid(out);
}
typedef struct {
    uint32_t magic, version;
    uint64_t ticket, pid, tick, sequence;
    uint32_t result, ready;
    char path[512];
} NshLoaderReceipt;
#ifdef __cplusplus
static_assert(sizeof(NshLoaderTicket) == 528, "loader ticket ABI");
static_assert(sizeof(NshLoaderReceipt) == 560, "loader receipt ABI");
#else
_Static_assert(sizeof(NshLoaderTicket) == 528, "loader ticket ABI");
_Static_assert(sizeof(NshLoaderReceipt) == 560, "loader receipt ABI");
#endif
