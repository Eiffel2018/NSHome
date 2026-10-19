/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stdint.h>
#define NSH_LOADER_TICKET "sdmc:/config/NSHome/breeze-ticket.bin"
#define NSH_LOADER_RECEIPT "sdmc:/config/NSHome/breeze-loader.bin"
#define NSH_BREEZE_PATH "sdmc:/switch/Breeze/Breeze.nro"
typedef struct {
    uint32_t magic, version;
    uint64_t ticket, pid, tick, sequence;
    uint32_t result, ready;
    char path[512];
} NshLoaderReceipt;
#ifdef __cplusplus
static_assert(sizeof(NshLoaderReceipt) == 560, "loader receipt ABI");
#else
_Static_assert(sizeof(NshLoaderReceipt) == 560, "loader receipt ABI");
#endif
