// SPDX-License-Identifier: MIT
#include "../../Source/Teensy/MinimalBoot/Common/VMImageLoad.h"
#include <cassert>
#include <vector>
#include <fstream>
#include <cstdio>
struct Reader {
    const std::vector<uint8_t> &bytes;size_t offset=64;int calls=0,shortAt=0;
    int read(void *p,uint32_t n){++calls;if(calls==shortAt)return -1;if(n>bytes.size()-offset)return -1;memcpy(p,bytes.data()+offset,n);offset+=n;return n;}
};
static void seal(VmImageHeader &h){h.header_crc=0;h.header_crc=vm_crc32(&h,sizeof h);}
int main(int argc,char **argv){
    assert(argc==2);std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> image{std::istreambuf_iterator<char>(f),{}};
    assert(image.size()>64);VmImageHeader good;memcpy(&good,image.data(),64);
    assert(vm_valid_header(good,image.size())&&good.reserved[0]==VM_PROFILE_RAM2_RO96);
    assert(vm_image_guest_bytes(good)==425984&&vm_image_ro_bytes(good)<=98304);
    assert(VM_RAM2_RO_BASE==VM_RAM_BASE+vm_image_guest_bytes(good));
    auto reject=[&](VmImageHeader h,uint32_t size=0){seal(h);assert(!vm_valid_header(h,size?size:image.size()));};
    for(unsigned i=0;i<64;i++){auto h=good;((uint8_t *)&h)[i]^=0x80;assert(!vm_valid_header(h,image.size()));}
    for(uint32_t n:{0u,63u,uint32_t(image.size()-1),uint32_t(image.size()+1)})assert(!vm_valid_header(good,n));
    auto h=good;h.reserved[0]=2;reject(h);h=good;h.reserved[1]=0;reject(h);h=good;h.reserved[1]=98305;reject(h);
    h=good;h.reserved[1]=UINT32_MAX;reject(h);h=good;h.required_services&=~VM_SERVICE_RAM2_RO;reject(h);
    h=good;h.reserved[2]=1;reject(h);h=good;h.reserved[3]=1;reject(h);
    h=good;h.code_bytes=UINT32_MAX;reject(h);h=good;h.data_bytes=UINT32_MAX;reject(h);
    h=good;h.bss_bytes=VM_DATA_BYTES-h.data_bytes+1;reject(h);
    h=good;h.entry=VM_RAM2_RO_BASE|1;reject(h);h=good;h.ram_base=VM_RAM2_RO_BASE;reject(h);
    // Legacy modules retain the old exact file size and all 512 KiB RAM2.
    h=good;h.reserved[0]=h.reserved[1]=0;h.required_services&=~VM_SERVICE_RAM2_RO;seal(h);
    uint32_t legacyBytes=64+h.code_bytes+h.data_bytes;
    assert(vm_valid_header(h,legacyBytes)&&vm_image_guest_bytes(h)==VM_RAM_BYTES);
    h.reserved[1]=32;reject(h,legacyBytes);h.reserved[1]=0;h.required_services|=VM_SERVICE_RAM2_RO;reject(h,legacyBytes);
    std::vector<uint8_t> code(98304+64,0x55),data(VM_DATA_BYTES+64,0x66),ram2(VM_RAM_BYTES+64,0x77);
    auto check=[&](){
        for(unsigned i=0;i<64;i++){assert(code[98304+i]==0x55&&data[VM_DATA_BYTES+i]==0x66&&ram2[VM_RAM_BYTES+i]==0x77);}
        for(unsigned i=0;i<VM_RAM2_GUEST_BYTES;i++)assert(ram2[i]==0x77);
    };
    Reader reader{image};uint8_t failure=0;
    assert(vm_load_payload(good,reader,code.data(),data.data(),ram2.data()+VM_RAM2_GUEST_BYTES,failure));
    assert(reader.offset==image.size()&&reader.calls==3&&!failure);check();
    assert(!memcmp(code.data(),image.data()+64,good.code_bytes));
    assert(!memcmp(data.data(),image.data()+64+good.code_bytes,good.data_bytes));
    assert(!memcmp(ram2.data()+VM_RAM2_GUEST_BYTES,image.data()+64+good.code_bytes+good.data_bytes,vm_image_ro_bytes(good)));
    for(unsigned i=0;i<good.bss_bytes;i++)assert(!data[good.data_bytes+i]);
    for(int part=1;part<=3;part++){Reader r{image};r.shortAt=part;failure=0;assert(!vm_load_payload(good,r,code.data(),data.data(),ram2.data()+VM_RAM2_GUEST_BYTES,failure)&&failure==0x12);check();}
    auto corrupt=image;corrupt.back()^=1;Reader bad{corrupt};failure=0;
    assert(!vm_load_payload(good,bad,code.data(),data.data(),ram2.data()+VM_RAM2_GUEST_BYTES,failure)&&failure==0x13);check();
    // Old profile's two-section payload cannot touch the new table area.
    VmImageHeader legacy=good;legacy.reserved[0]=legacy.reserved[1]=0;legacy.required_services&=~VM_SERVICE_RAM2_RO;
    std::vector<uint8_t> old(image.begin(),image.begin()+legacyBytes);legacy.payload_crc=vm_crc32(old.data()+64,old.size()-64);seal(legacy);
    std::fill(ram2.begin(),ram2.end(),0x77);Reader oldReader{old};failure=0;
    assert(vm_load_payload(legacy,oldReader,code.data(),data.data(),nullptr,failure));
    for(auto v:ram2)assert(v==0x77);check();
    puts("PASS: legacy/profile headers, 3-section loader, payload CRC, bounds, short reads and guest/table isolation");
}
