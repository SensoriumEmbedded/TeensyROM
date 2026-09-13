#pragma once
#include "native-display.h"
#include "native-data.h"
#include "native-cache.h"

namespace dosvm_nuflix {
inline unsigned templateWord(unsigned offset) { return displayTemplate[offset] | (displayTemplate[offset+1]<<8); }
NUFLIX_CODE inline uint8_t displayByte(const NativeDisplay &display,unsigned address) {
    unsigned mapped=displayMapping[address-0x1000];
    uint8_t value=mapped&0x8000?displayTemplate[mapped&0x7fff]:reinterpret_cast<const uint8_t *>(&display)[mapped];
    if(address<0x3000||address>=0x3400)return value;
#if defined(MPE_DOS_NUFLIX_DOUBLE)
    const unsigned patch=displayPatches[address-0x3000];
    if(patch>=0x10&&patch<0x14)return display.fit.bug[patch-0x10];
    if(patch>=0x20&&patch<0x26)return display.fit.underlay[patch-0x20];
    switch(patch){
        case 1:return display.codeBase?uint8_t(display.codeBase):value;
        case 2:return display.codeBase?uint8_t(display.codeBase>>8):value;
        case 0x30:return display.initialX;
        case 0x31:return display.initialY;
        case 0x32:return uint8_t(display.bankOperand);
        case 0x33:return uint8_t(display.bankOperand>>8);
        case 0x34:return 0x2c;
        case 0x35:return 0;
        default:return value;
    }
#else
    if(display.codeBase){
        if(address==0x3118||address==0x31f7)return uint8_t(display.codeBase);
        if(address==0x3119||address==0x31f8)return uint8_t(display.codeBase>>8);
    }
    for(unsigned standard=0;standard<2;++standard){
        for(unsigned slot=2;slot<12;++slot)if(address==templateWord((slot+standard*12)*2)){
            if(slot==2)return display.fit.bug[1];
            if(slot==3)return display.fit.bug[3];
            if(slot==4)return display.fit.bug[0];
            if(slot==11)return display.fit.bug[2];
            return display.fit.underlay[slot-5];
        }
        if(address==templateWord(24+standard*24))return display.initialX;
        if(address==templateWord(26+standard*24))return display.initialY;
    }
    if(address==templateWord(2))return uint8_t(display.bankOperand);
    if(address==templateWord(2)+1)return uint8_t(display.bankOperand>>8);
    if(address==templateWord(52))return 0x2c;
    if(address==templateWord(54) || address==templateWord(56))return 0;
    return value;
#endif
}
}
#if defined(MPE_DOS_NUFLIX_DOUBLE)
#include "double-export.h"
#endif
