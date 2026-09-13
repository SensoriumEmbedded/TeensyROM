#pragma once
#include "double-data.h"

namespace dosvm_nuflix {
NUFLIX_CODE inline unsigned mirroredIndexedOperand(const NativeDisplay &display){
    unsigned offset=display.bankOperand-0x1000;
    if(display.code[offset-1]!=0xa2)return 0;
    for(++offset;offset<sizeof(display.code);){
        const unsigned opcode=display.code[offset];
        if(opcode==0xa2||opcode==0xa6||opcode==0x60)return 0;
        if(opcode==0x9d)return 0x1001+offset;
        if(opcode==0xea)++offset;
        else if(opcode==0x8d||opcode==0x8e||opcode==0x8c||opcode==0x2c)offset+=3;
        else offset+=2;
    }
    return 0;
}
NUFLIX_CODE inline uint8_t bankDisplayByte(const NativeDisplay &display,unsigned address,unsigned bank,unsigned indexedOperand){
    uint8_t value=displayByte(display,address);
    if(!bank)return value;
    if(address==display.bankOperand)return 1;
    if(address==indexedOperand)return uint8_t(value+2);
    if(address>=0x3000&&address<0x4200)return uint8_t(value+mirrorDeltas[address-0x3000]);
    return value;
}
}
