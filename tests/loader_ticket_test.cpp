// SPDX-License-Identifier: GPL-2.0-only
#include "loader_receipt.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>

int main() {
    NshLoaderTicket ticket{};
    ticket.magic=0x5448534e; ticket.version=2; ticket.ticket=123;
    std::snprintf(ticket.path, sizeof(ticket.path), "%s", "sdmc:/switch/My Tool/test.NRO");
    NshLoaderTicket decoded{};
    assert(nshLoaderTicketDecode(&ticket, sizeof(ticket), &decoded));
    assert(decoded.ticket==123 && !std::strcmp(decoded.path,ticket.path));
    for (const char* path : {"sdmc:/../x.nro", "sdmc:/switch/./x.nro", "sdmc:/switch//x.nro", "sdmc:/x.nsp", "sdmc:/x\\a.nro", "sdmc:/x\na.nro", "sdmc:/x:y.nro", "/switch/a.nro"}) {
        std::snprintf(ticket.path, sizeof(ticket.path), "%s", path);
        assert(!nshLoaderTicketValid(&ticket));
    }
    std::memset(ticket.path,'x',sizeof(ticket.path));
    assert(!nshLoaderTicketValid(&ticket));
    std::snprintf(ticket.path,sizeof(ticket.path),"%s",NSH_BREEZE_PATH);
    for (size_t n=0;n<sizeof(ticket);++n) assert(!nshLoaderTicketDecode(&ticket,n,&decoded));
    const uint64_t legacy=321;
    assert(nshLoaderTicketDecode(&legacy,sizeof(legacy),&decoded));
    assert(decoded.ticket==legacy && !std::strcmp(decoded.path,NSH_BREEZE_PATH));
    const uint64_t zero=0;
    assert(!nshLoaderTicketDecode(&zero,sizeof(zero),&decoded));
    std::puts("loader ticket: requested paths, bounds, traversal and legacy ABI passed");
}
