#include <cassert>
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdio>
#include "../abi/vm_abi.h"
int main(int argc,char **argv){
    assert(argc==2);std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t>b{std::istreambuf_iterator<char>(f),{}};
    assert(b.size()>64);VmImageHeader good;memcpy(&good,b.data(),64);assert(vm_valid_header(good,b.size()));assert(vm_crc32(b.data()+64,b.size()-64)==good.payload_crc);
    for(unsigned i=0;i<64;i++){auto h=good;((uint8_t *)&h)[i]^=0x80;assert(!vm_valid_header(h,b.size()));}
    for(uint32_t size:{0u,63u,(uint32_t)b.size()-1,(uint32_t)b.size()+1})assert(!vm_valid_header(good,size));
    auto reject=[&](VmImageHeader h){h.header_crc=0;h.header_crc=vm_crc32(&h,64);assert(!vm_valid_header(h,b.size()));};
    auto video=good;video.required_services=VM_SERVICE_VIDEO;video.header_crc=0;video.header_crc=vm_crc32(&video,64);assert(vm_valid_header(video,b.size()));
    auto h=good;h.code_bytes=0xffffffff;reject(h);h=good;h.data_bytes=0xffffffff;reject(h);h=good;h.bss_bytes=VM_RAM_BYTES+1;reject(h);
    h=good;h.entry=VM_CODE_BASE-1;reject(h);h=good;h.entry&=~1;reject(h);h=good;h.entry=VM_CODE_LIMIT|1;reject(h);h=good;h.required_services=0x80000000u;reject(h);
    h=good;h.abi++;reject(h);h=good;h.ram_base=0x20000000;reject(h);
    b.back()^=1;assert(vm_crc32(b.data()+64,b.size()-64)!=good.payload_crc);
    puts("PASS: MVM1 image CRC, 64 header corruption cases, truncation, overflow, ABI, imports, entry and arena bounds");
}
