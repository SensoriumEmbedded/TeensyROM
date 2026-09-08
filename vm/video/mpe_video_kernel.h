// SPDX-License-Identifier: MIT
#pragma once
#include "mpe_video_live.h"
#ifndef MPE_VIDEO_CODE
#define MPE_VIDEO_CODE
#endif
namespace mpe_video {
constexpr unsigned KernelCapacity=0x1000;
// Two independently executable kernels fit at $3000 and $c000. Shared delay
// subroutines replace long NOP runs without changing a single raster cycle.
// Entry remains raster 51 cycle 2; the receiver installs the $02e0 exit.
static MPE_VIDEO_CODE unsigned buildKernel(const LiveFrame &frame,bool ntsc,uint8_t *buffer,unsigned capacity,uint16_t base=0x3000){
    if(!buffer||capacity<KernelCapacity||(base!=0x3000&&base!=0xc000))return 0;
    uint8_t *p=buffer;
    uint16_t fixup[200]{},helper[54]{};uint8_t waits[200]{};
    for(unsigned y=0;y<200;y++){
        const unsigned band=y/8,row=y&7;
        const bool enhanced=(frame.mask&(1u<<band))!=0;
        if(enhanced&&(frame.split[band]<1||frame.split[band]>7))return 0;
        const bool split=enhanced&&row==frame.split[band];
        // VIC bank $8000 has character ROM at $9000-$9fff. Its screen maps
        // must live at $8800/$8c00, not in that hardware shadow.
        *p++=0xa9;*p++=(enhanced&&row>=frame.split[band]?0x68:0x78)-(base==0xc000?0x40:0);
        *p++=0x8d;*p++=0x18;*p++=0xd0;
        *p++=0xa9;*p++=split?uint8_t(0x38|((51+y)&7)):0x3b;
        *p++=0x8d;*p++=0x11;*p++=0xd0;
        unsigned delay=(ntsc?65:63)-12-(row==0?43:split?40:0);
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
