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
    auto accept=[&](VmImageHeader h){h.header_crc=0;h.header_crc=vm_crc32(&h,64);assert(vm_valid_header(h,b.size()));};
    // Requiring a service this loader does not provide is well formed.
    for(uint32_t other:{32u,64u,256u,512u,8192u,0x10000u,0x80000000u}){auto h=good;h.required_services|=other;accept(h);}
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
    h=good;h.entry=VM_CODE_BASE-1;reject(h);h=good;h.entry&=~1;reject(h);h=good;h.entry=VM_CODE_LIMIT|1;reject(h);
    h=good;h.abi++;reject(h);h=good;h.ram_base=0x20000000;reject(h);h=good;h.code_base=0;reject(h);
    h=good;h.reserved[2]=1;reject(h);h=good;h.reserved[3]=1;reject(h);
    // The second code base. Every bound is measured from the image's own base,
    // so the same module is well formed at either one once entry moves with it.
    auto sized=[&](VmImageHeader s,bool want){const uint32_t n=64+vm_image_payload_bytes(s);
        s.header_crc=0;s.header_crc=vm_crc32(&s,64);assert(vm_valid_header(s,n)==want);};
    const uint32_t delta=VM_CODE_BASE-VM_CODE_BASE_128K;
    h=good;h.code_base=VM_CODE_BASE_128K;h.entry=good.entry-delta;sized(h,true);
    // Entry left in the window the image no longer occupies.
    h=good;h.code_base=VM_CODE_BASE_128K;sized(h,false);
    // The extra 32 KiB is reachable only from the lower base.
    h=good;h.code_base=VM_CODE_BASE_128K;h.entry=good.entry-delta;
    h.code_bytes=VM_CODE_LIMIT-VM_CODE_BASE_128K;sized(h,true);
    h.code_bytes++;sized(h,false);
    h=good;h.code_bytes=VM_CODE_LIMIT-VM_CODE_BASE;sized(h,true);
    h=good;h.code_bytes=VM_CODE_LIMIT-VM_CODE_BASE_128K;sized(h,false);
    // No third base. 0x14000 is the one that matters: its region would be
    // rounded down over host code by the MPU rather than refused. entry and
    // code_bytes are fitted to each base so that every other bound holds and
    // only the base check can refuse it; an odd base needs entry one halfword
    // up, and 0x2fffe leaves room for two bytes of code.
    for(uint32_t base:{0u,0x10001u,0x14000u,0x18001u,0x20000u,0x2fffeu}){
        h=good;h.code_base=base;h.entry=((base+1)&~1u)|1;
        if(h.code_bytes>VM_CODE_LIMIT-base)h.code_bytes=VM_CODE_LIMIT-base;
        sized(h,false);}
    b.back()^=1;assert(vm_crc32(b.data()+64,b.size()-64)!=good.payload_crc);
    puts("PASS: MVM1 image CRC, 64 header corruption cases, truncation, overflow, ABI, entry/arena bounds, "
         "both code bases and the six illegal ones, profile consistency, and services outside this loader "
         "accepted as well formed");
}
