// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
namespace mpe_video {
// Camera advances only while preparing a new image, never during a frozen
// DMA/ACK generation. Input is held state; opposite directions cancel.
struct CropCamera {
    static constexpr uint32_t RepeatUs=80000;
    uint16_t x=0,y=0,width=0,height=0;
    uint8_t held=0,mode=0;
    uint32_t at=0;
    bool initialized=false;
    void input(uint8_t selection,uint8_t keys,uint32_t now){
        if(selection!=mode){mode=selection;initialized=false;held=0;}
        keys=selection==1?(keys&15):0;
        if(keys!=held){held=keys;at=now-RepeatUs;}
    }
    void position(uint16_t w,uint16_t h,uint32_t now){
        if(w<160||h<200)return;
        if(!initialized||width!=w||height!=h){
            width=w;height=h;x=(w-160)/2;y=(h-200)/2;initialized=true;at=now-RepeatUs;
        }
        if(!held){at=now;return;}
        unsigned steps=uint32_t(now-at)/RepeatUs;if(!steps)return;
        if(steps>4)steps=4;at=now;
        const int dx=(bool(held&8)-bool(held&4))*int(steps*4);
        const int dy=(bool(held&2)-bool(held&1))*int(steps*4);
        auto clamp=[](int n,int limit){return uint16_t(n<0?0:n>limit?limit:n);};
        x=clamp(int(x)+dx,w-160);y=clamp(int(y)+dy,h-200);
    }
};
}
