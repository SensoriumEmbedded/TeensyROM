// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
namespace mpe_video { namespace sprites {
// Eight physical, unexpanded hires sprites; no raster multiplexing or FLI.
struct Sprite {uint16_t x;uint8_t y,color;uint32_t score;uint16_t priority;uint8_t data[64];};
struct Plan {Sprite sprites[8];unsigned count;};
// Source reader: C64 color in bits 0..3, optional actor hint in bit 4.
// Disjoint grid windows and source-color masks make every correction exact;
// opaque pixels cannot cover another correction or an already correct pixel.
template<class Source,class Base,class Loss>
inline Plan select(Source source,Base base,Loss loss){
    Plan out{};
    for(unsigned y=0;y<200;y+=21)for(unsigned x=0;x<320;x+=24){
        uint32_t score[16]{};uint16_t focus[16]{};
        for(unsigned dy=0;dy<21&&y+dy<200;dy++)for(unsigned dx=0;dx<24&&x+dx<320;dx++){
            const unsigned i=(y+dy)*320+x+dx;const auto sample=source(i),shown=base(i),want=uint8_t(sample&15);
            score[want]+=loss(want,shown);if((sample&16)&&want!=shown)focus[want]++;
        }
        for(unsigned color=0;color<16;color++)if(score[color]){
            unsigned at=0;while(at<out.count&&(out.sprites[at].priority>focus[color]||
                (out.sprites[at].priority==focus[color]&&out.sprites[at].score>=score[color])))at++;
            if(at==8)continue;if(out.count<8)out.count++;
            for(unsigned j=out.count-1;j>at;j--)out.sprites[j]=out.sprites[j-1];
            out.sprites[at]={uint16_t(x),uint8_t(y),uint8_t(color),score[color],focus[color],{}};
        }
    }
    for(unsigned n=0;n<out.count;n++){
        auto &s=out.sprites[n];
        for(unsigned y=0;y<21&&s.y+y<200;y++)for(unsigned x=0;x<24&&s.x+x<320;x++){
            const unsigned i=(s.y+y)*320+s.x+x;
            if((source(i)&15)==s.color&&base(i)!=s.color)s.data[y*3+x/8]|=128>>(x%8);
        }
    }
    return out;
}
// The unused second 1000-byte attribute plane contains 512 pattern bytes,
// 16 position bytes, X-MSB, enable mask, eight colors, then zero padding.
// Pattern pointers $60..$67 refer to $5800..$59ff in VIC bank $4000.
template<class Write> inline void pack(const Plan &p,Write write){
    for(unsigned i=0;i<1000;i++)write(i,0);
    uint8_t msb=0,enabled=0;
    for(unsigned n=0;n<p.count;n++){
        const auto &s=p.sprites[n];for(unsigned i=0;i<64;i++)write(n*64+i,s.data[i]);
        const unsigned x=s.x+24;write(512+n*2,uint8_t(x));write(513+n*2,s.y+50);
        if(x>255)msb|=1<<n;enabled|=1<<n;write(530+n,s.color);
    }
    write(528,msb);write(529,enabled);
}
}}
