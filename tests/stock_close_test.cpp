// SPDX-License-Identifier: GPL-2.0-only
#include "close_screen.hpp"
#include <vector>
#include <cassert>
int main() {
    std::vector<std::uint8_t> image(1280*720,17);
    auto pixel=[&](unsigned x,unsigned y)->std::uint8_t& { return image[y*1280+x]; };
    pixel(985,685)=45;
    for(auto x:{975u,995u}) pixel(x,685)=255;
    for(auto y:{675u,695u}) pixel(985,y)=255;
    assert(observer::stock_close_image(image.data(),1280,720,false));
    assert(!observer::stock_close_image(image.data(),1280,720,true));
    pixel(975,685)=45;
    assert(!observer::stock_close_image(image.data(),1280,720,false));
    image.assign(image.size(),17);
    for(unsigned y=190;y<530;++y) for(unsigned x=280;x<1030;++x) pixel(x,y)=70;
    for(auto x:{700u,900u}) pixel(x,458)=150;
    for(auto x:{638u,1028u}) pixel(x,490)=150;
    assert(!observer::stock_close_image(image.data(),1280,720,true));
    pixel(340,260)=10; pixel(460,380)=200;
    assert(observer::stock_close_image(image.data(),1280,720,true));
    // Opposite luminance (stock light theme) preserves structural checks.
    for(auto& p:image) p=255-p;
    assert(observer::stock_close_image(image.data(),1280,720,true));
    pixel(638,490)=185;
    assert(!observer::stock_close_image(image.data(),1280,720,true));
    assert(!observer::stock_close_image(nullptr,1280,720,true));
    assert(!observer::stock_close_image(image.data(),720,1280,true));
}
