// SPDX-License-Identifier: MIT
// Synthetic AUX image and actual stock EasyFlash guards, without a DOS engine.
#include "../../Source/Teensy/MinimalBoot/Common/VMImageLoad.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>

#define MinimumBuild
#define FeatVMHost
#define Printf_Swaps(...) ((void)0)
static constexpr unsigned Num8kSwapBuffers=2;
static constexpr uint32_t SwapSeekAddrMask=0x80000000;
struct SwapBuffer {uint8_t Image[8192];uint32_t Offset;};
static SwapBuffer SwapBuffers[Num8kSwapBuffers];
static_assert(sizeof(SwapBuffers)>=16384,"retired swap storage holds the second AUX span");
enum {DMA_S_DisableReady,DMA_S_StartImmediate,DMA_S_ActiveReady,DMA_S_StartDisable};
static unsigned DMA_State,hostPolls,bankLoads;
static uint8_t *LOROM_Image,*HIROM_Image;
static bool owned;
static bool VMHostOwnsSwapRAM(){return owned;}
static void VMHostPoll(){++hostPolls;}
static unsigned millis(){return 100;}
static void LoadBank(uint32_t offset,uint8_t* destination){
    assert(offset==8192);++bankLoads;memset(destination,0x42,8192);
}
static struct {void flush(){}} Serial;
#include "easyflash-swap-under-test.h"

struct Reader {
    const std::vector<uint8_t>& bytes;size_t offset=0;unsigned reads=0;
    int read(void* destination,uint32_t count){
        ++reads;if(count>bytes.size()-offset)return -1;
        memcpy(destination,bytes.data()+offset,count);offset+=count;return count;
    }
};
static void seal(VmImageHeader& h){h.header_crc=0;h.header_crc=vm_crc32(&h,sizeof h);}
int main(){
    VmImageHeader good{};
    good.magic=VM_IMAGE_MAGIC;good.abi=VM_ABI;good.header_bytes=sizeof good;
    good.code_bytes=VM_AUX_CODE_LIMIT-VM_CODE_BASE;good.data_bytes=32;good.bss_bytes=128;
    good.entry=VM_CODE_BASE|1;good.code_base=VM_CODE_BASE;good.ram_base=VM_DATA_BASE;
    good.required_services=VM_SERVICES|VM_SERVICE_RAM1_AUX;good.reserved[0]=VM_PROFILE_RAM1_AUX;
    std::vector<uint8_t> payload(good.code_bytes+good.data_bytes,0x47);
    good.payload_crc=vm_crc32(payload.data(),payload.size());seal(good);
    const uint32_t fileBytes=sizeof good+payload.size();
    assert(vm_valid_header(good,fileBytes));
    assert(vm_image_guest_bytes(good)==512*1024&&vm_image_ro_bytes(good)==0);
    assert(good.code_bytes==64*1024&&VM_CODE_LIMIT-VM_AUX_CODE_LIMIT==32*1024);
    const auto reject=[&](VmImageHeader h){seal(h);assert(!vm_valid_header(h,fileBytes));};
    auto h=good;h.code_bytes++;reject(h);
    h=good;h.entry=VM_AUX_CODE_LIMIT|1;reject(h);
    h=good;h.required_services&=~VM_SERVICE_RAM1_AUX;reject(h);
    h=good;h.required_services|=VM_SERVICE_RAM2_RO;reject(h);
    h=good;h.reserved[0]=VM_PROFILE_LEGACY;reject(h);
    h=good;h.reserved[1]=32;reject(h);
    h=good;h.reserved[2]=1;reject(h);
    h=good;h.data_bytes=VM_DATA_BYTES;reject(h);
    // The legacy profile can still use the complete original code window.
    h=good;h.reserved[0]=VM_PROFILE_LEGACY;h.required_services&=~VM_SERVICE_RAM1_AUX;
    h.code_bytes=VM_CODE_LIMIT-VM_CODE_BASE;seal(h);
    assert(vm_valid_header(h,sizeof h+h.code_bytes+h.data_bytes));

    std::vector<uint8_t> code(VM_CODE_LIMIT-VM_CODE_BASE+32,0xa5);
    std::vector<uint8_t> data(VM_DATA_BYTES+32,0xb6),ram2(VM_RAM_BYTES+32,0xc7);
    Reader reader{payload};uint8_t failure=0;
    assert(vm_load_payload(good,reader,code.data(),data.data(),ram2.data(),failure));
    assert(reader.reads==2&&reader.offset==payload.size()&&!failure);
    assert(!memcmp(code.data(),payload.data(),good.code_bytes));
    assert(!memcmp(data.data(),payload.data()+good.code_bytes,good.data_bytes));
    for(unsigned i=good.data_bytes;i<good.data_bytes+good.bss_bytes;i++)assert(data[i]==0);
    for(size_t i=good.code_bytes;i<code.size();i++)assert(code[i]==0xa5);
    for(size_t i=good.data_bytes+good.bss_bytes;i<data.size();i++)assert(data[i]==0xb6);
    for(auto byte:ram2)assert(byte==0xc7);
    payload.back()^=1;Reader corrupt{payload};
    assert(!vm_load_payload(good,corrupt,code.data(),data.data(),nullptr,failure)&&failure==0x13);
    payload.pop_back();Reader shortRead{payload};failure=0;
    assert(!vm_load_payload(good,shortRead,code.data(),data.data(),nullptr,failure)&&failure==0x12);

    auto requested=reinterpret_cast<uint8_t*>(uintptr_t(SwapSeekAddrMask|8192));
    memset(SwapBuffers,0,sizeof SwapBuffers);owned=false;DMA_State=DMA_S_DisableReady;
    assert(ImageCheckAssign(requested)==requested&&DMA_State==DMA_S_StartImmediate);
    SwapBuffers[0].Offset=SwapSeekAddrMask|8192;DMA_State=DMA_S_DisableReady;
    assert(ImageCheckAssign(requested)==SwapBuffers[0].Image&&DMA_State==DMA_S_DisableReady);
    owned=true;assert(ImageCheckAssign(requested)==requested&&DMA_State==DMA_S_DisableReady);
    std::vector<uint8_t> scratch(sizeof SwapBuffers);memcpy(scratch.data(),SwapBuffers,sizeof SwapBuffers);
    LOROM_Image=requested;HIROM_Image=nullptr;DMA_State=DMA_S_ActiveReady;
    PollingHndlr_EasyFlash();
    assert(hostPolls==1&&bankLoads==0&&DMA_State==DMA_S_ActiveReady&&LOROM_Image==requested);
    assert(!memcmp(scratch.data(),SwapBuffers,sizeof SwapBuffers));
    owned=false;PollingHndlr_EasyFlash();
    assert(hostPolls==2&&bankLoads==1&&DMA_State==DMA_S_StartDisable);
    assert(LOROM_Image==SwapBuffers[0].Image&&SwapBuffers[0].Offset==(SwapSeekAddrMask|8192));
    for(auto byte:SwapBuffers[0].Image)assert(byte==0x42);
    puts("PASS: RAM1_AUX headers/services, 64 KiB code limit, loader CRC/bounds, untouched 32 KiB ITCM tail/RAM2, and stock EasyFlash swap ownership with legacy fallthrough");
}
