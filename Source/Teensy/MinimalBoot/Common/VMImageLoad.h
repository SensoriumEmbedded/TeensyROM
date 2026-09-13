// SPDX-License-Identifier: MIT
#pragma once
#include "VMABI.h"
#include <string.h>
// Caller validates the header before granting write access. The same payload
// reader is tested on the host and used by firmware; no image-selected address.
template<class Reader> static bool vm_load_payload(const VmImageHeader &h,Reader &reader,
        uint8_t *code,uint8_t *data,uint8_t *ro,uint8_t &failure){
    uint32_t crc=~0u;
    for(unsigned part=0;part<3;part++){
        auto p=part==0?code:part==1?data:ro;
        uint32_t n=part==0?h.code_bytes:part==1?h.data_bytes:vm_image_ro_bytes(h);
        if(n&&(!p||reader.read(p,n)!=(int)n)){failure=0x12;return false;}
        while(n--){crc^=*p++;for(unsigned b=0;b<8;b++)crc=(crc>>1)^((0u-(crc&1))&0xedb88320u);}
    }
    if(~crc!=h.payload_crc){failure=0x13;return false;}
    memset(data+h.data_bytes,0,h.bss_bytes);return true;
}
