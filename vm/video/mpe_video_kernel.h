// SPDX-License-Identifier: MIT
#pragma once
#include "mpe_video_live.h"
#ifndef MPE_VIDEO_CODE
#define MPE_VIDEO_CODE
#endif
namespace mpe_video {
constexpr unsigned KernelCapacity=0x1000;
// One solid sprite pattern and five pointers in each of the four screen maps.
// The fitted full-picture profile leaves these 24 columns unused. Its five
// sprites share one pattern; their raster-controlled expansion ends all DMA
// before the border grant instead of fetching transparent trailing rows.
constexpr unsigned FullMaskBytes=64+4*5;
inline uint8_t fullMaskByte(unsigned offset){return offset<64?(offset<63?0xff:0):0xfd;}
inline uint16_t fullMaskAddress(unsigned bank,unsigned offset){
    return offset<64?(bank?0xbf40:0x7f40)+offset:
        (bank?0x8ff8:0x5ff8)-((offset-64)/5)*0x400+(offset-64)%5;
}
// Two independently executable kernels fit at $3000 and $c000. Shared delay
// subroutines replace long NOP runs without changing a single raster cycle.
// Entry remains raster 51 cycle 2; the receiver installs the $02e0 exit.
static MPE_VIDEO_CODE unsigned buildKernel(const LiveFrame &frame,bool ntsc,uint8_t *buffer,unsigned capacity,uint16_t base=0x3000,bool center=false,bool maskCenter=true,bool fullMask=false){
    if(!buffer||capacity<KernelCapacity||(base!=0x3000&&base!=0xc000))return 0;
    if(fullMask&&(!center||maskCenter||frame.mask!=0x1ffffff))return 0;
    uint8_t *p=buffer;
    // The legacy center profile covers the VIC's first three FLI columns
    // with sprites. Four-map output can span all 25 character rows, with
    // the bottom fetch exception below. The full mask has its own timing and
    // never changes the legacy center profile's sprite placement or cadence.
    const bool masked=center&&maskCenter;
    unsigned centerFirst=0;if(masked){while(centerFirst<25&&!(frame.mask&(1u<<centerFirst)))++centerFirst;if(centerFirst<1||centerFirst>14)return 0;}
    uint16_t fixup[200]{},helper[54]{};uint8_t waits[200]{};
    for(unsigned y=0;y<200;y++){
        const unsigned band=y/8,row=y&7;
        const bool enhanced=(frame.mask&(1u<<band))!=0;
        if(enhanced&&(frame.split[band]<1||frame.split[band]>7))return 0;
        // VIC badlines end at raster 247, before the final pair at 249.
        // Full-height output uses one pair for its last four lines.
        const bool bottomPair=center&&!maskCenter&&y>=198;
        const bool split=enhanced&&(center?(row!=0&&!(row&1)&&!bottomPair):row==frame.split[band]);
        // VIC bank $8000 has character ROM at $9000-$9fff. Its screen maps
        // must live at $8800/$8c00, not in that hardware shadow.
        *p++=0xa9;*p++=(center?(0x78-(enhanced?(bottomPair?2:row/2)*0x10:0)):(enhanced&&row>=frame.split[band]?0x68:0x78))-(base==0xc000?0x40:0);
        *p++=0x8d;*p++=0x18;*p++=0xd0;
        *p++=0xa9;*p++=split?uint8_t(0x38|((51+y)&7)):0x3b;
        *p++=0x8d;*p++=0x11;*p++=0xd0;
        unsigned delay=(ntsc?65:63)-12-(row==0?43:split?40:0);
        // First two fitted margin lines are already black in the bitmap.
        // Start sprite DMA only on line 52, after the stabilized entry, so it
        // cannot perturb the IRQ prelude. Restore expansion on every frame.
        if((masked||fullMask)&&y==1){*p++=0xa9;*p++=fullMask?31:3;*p++=0x8d;*p++=0x15;*p++=0xd0;delay-=6;
            if(fullMask){*p++=0x8d;*p++=0x17;*p++=0xd0;delay-=4;}}
        // Sprite 0's DMA delays the natural badline write by five cycles;
        // sprite 1 starts later. PAL's short post-badline instruction sequence
        // loses four rather than five cycles with sprite 1. Forced fetches
        // still write D011 at cycle 14 on both video standards.
        if(masked&&y>=centerFirst*8-1&&y<centerFirst*8+83)
            delay-=!ntsc&&y>=centerFirst*8+41&&row==0?4:5;
        if(fullMask&&y>=1)delay-=!ntsc&&y>=43&&y<85&&row==0?4:5;
        // Last sprite spans 30 lines: nine expanded pattern rows followed
        // by twelve ordinary rows. Its MCBASE reaches $3f at visible end;
        // clearing D015 alone would not stop already active sprite DMA.
        if(fullMask&&y==187){*p++=0xa9;*p++=15;*p++=0x8d;*p++=0x17;*p++=0xd0;delay-=6;}
        // A split on row 7 must restore ordinary YSCROLL before the next
        // line's early badline test. Waiting for next line cycle 14 is late.
        if(split&&row==7){*p++=0xa9;*p++=0x3b;*p++=0x8d;*p++=0x11;*p++=0xd0;delay-=6;}
        if(delay==12||delay>=14){
            waits[y]=delay;helper[delay]=1;*p++=0x20;
            fixup[y]=unsigned(p-buffer);*p++=0;*p++=0;
        }else{
            if(delay&1){*p++=0x24;*p++=0x03;delay-=3;}
            while(delay){*p++=0xea;delay-=2;}
        }
    }
    if(fullMask){*p++=0xa9;*p++=0;*p++=0x8d;*p++=0x15;*p++=0xd0;}
    *p++=0x4c;*p++=0xe0;*p++=0x02;
    for(unsigned delay=12;delay<54;delay++)if(helper[delay]){
        helper[delay]=base+unsigned(p-buffer);
        unsigned cycles=delay-12; // JSR + RTS are twelve cycles.
        if(cycles&1){*p++=0x24;*p++=0x03;cycles-=3;}
        while(cycles){*p++=0xea;cycles-=2;}
        *p++=0x60;
    }
    for(unsigned y=0;y<200;y++)if(waits[y]){
        const auto address=helper[waits[y]];buffer[fixup[y]]=uint8_t(address);buffer[fixup[y]+1]=address>>8;
    }
    return unsigned(p-buffer);
}
}
