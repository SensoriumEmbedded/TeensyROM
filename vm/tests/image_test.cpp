// SPDX-License-Identifier: MIT
// Runs the firmware's own vm_valid_header against a real image built by
// tools/lib/extension.mjs, so the JavaScript writer and the C++ reader are
// checked against each other rather than each against itself.
#include <cassert>
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdio>
#include "../abi/vm_abi.h"
int main(int argc,char **argv){
    assert(argc==2);std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t>b{std::istreambuf_iterator<char>(f),{}};
    assert(b.size()>64);VmImageHeader good;memcpy(&good,b.data(),64);
    assert(vm_valid_header(good,b.size()));assert(vm_crc32(b.data()+64,b.size()-64)==good.payload_crc);
    // Every single-bit-flipped header must be refused, including in the CRC itself.
    for(unsigned i=0;i<64;i++){auto h=good;((uint8_t *)&h)[i]^=0x80;assert(!vm_valid_header(h,b.size()));}
    for(uint32_t size:{0u,63u,(uint32_t)b.size()-1,(uint32_t)b.size()+1})assert(!vm_valid_header(good,size));
    auto reject=[&](VmImageHeader h){h.header_crc=0;h.header_crc=vm_crc32(&h,64);assert(!vm_valid_header(h,b.size()));};
    // The negotiation refusal: a module asking for a service this profile does
    // not implement is refused outright, never loaded with the service missing.
    // These are the bit numbers reserved for out-of-tree extensions.
    for(uint32_t reserved:{32u,64u,256u,512u}){auto h=good;h.required_services|=reserved;reject(h);}
    assert(good.reserved[0]==VM_PROFILE_LEGACY||good.reserved[0]==VM_PROFILE_RAM2_RO);
    {auto h=good;h.reserved[0]=VM_PROFILE_RESERVED_AUX;reject(h);}
    if(good.reserved[0]==VM_PROFILE_LEGACY){
      auto h=good;h.reserved[1]=32;reject(h);
      h=good;h.required_services|=VM_SERVICE_RAM2_RO;reject(h);
    }else{
      auto h=good;h.reserved[1]=0;reject(h);
      h=good;h.required_services&=~VM_SERVICE_RAM2_RO;reject(h);
      h=good;h.reserved[1]=VM_RAM2_RO_BYTES+1;reject(h);
    }
    auto h=good;h.code_bytes=0xffffffff;reject(h);h=good;h.data_bytes=0xffffffff;reject(h);h=good;h.bss_bytes=VM_RAM_BYTES+1;reject(h);
    h=good;h.entry=VM_CODE_BASE-1;reject(h);h=good;h.entry&=~1;reject(h);h=good;h.entry=VM_CODE_LIMIT|1;reject(h);h=good;h.required_services=0x80000000u;reject(h);
    h=good;h.abi++;reject(h);h=good;h.ram_base=0x20000000;reject(h);h=good;h.code_base=0;reject(h);
    h=good;h.reserved[2]=1;reject(h);h=good;h.reserved[3]=1;reject(h);
    b.back()^=1;assert(vm_crc32(b.data()+64,b.size()-64)!=good.payload_crc);
    puts("PASS: MVM1 image CRC, 64 header corruption cases, truncation, overflow, ABI, entry/arena bounds, "
         "profile consistency and refusal of all four reserved service bits");
}
