// SPDX-License-Identifier: MIT
#pragma once
#include "Common/VMFiles.h"
#include "Common/VMImageLoad.h"
namespace VmRuntime {
using namespace VmFiles;
static VmRegistry::Launch launch;
static VmRegistry::Manifest manifest;
static VmHost host;
static const VmModule *module;
static VmPacket packet;
static uint8_t sequence;
static bool pending;
static volatile bool started;
static volatile bool active,startRequested,inputPending;
static volatile VmInput input;
static uint8_t failure;
static volatile bool quietRequested;
static volatile bool packetReplayRequested;
static uint32_t sliceStarted;
static volatile uint8_t videoTiming;
static bool videoDmaEnabled;
static bool auxiliaryOwned;
#if defined(FeatVMVideoDMA) && defined(Fab04_FullDMACapable)
static constexpr uint32_t providedServices=VM_HOST_SERVICES;
#else
static constexpr uint32_t providedServices=VM_SERVICES|VM_SERVICE_RAM2_RO|VM_SERVICE_RAM1_AUX;
#endif
static void moduleFail(uint8_t error,uint32_t detail);
static VmVideoResult videoPresent(const VmVideoFrame *frame);
static bool configureIndexedVideo(const VmIndexedVideoSetup *setup);
static VmVideoResult submitIndexedVideo(VmIndexedFrame *frame);
static void codeAccess(bool loading,bool auxiliary=false){
    // Core region 1 makes all ITCM read-only. A higher-priority region grants
    // only the module window RW+XN while loading, then restores RO+execute.
    uint32_t mask;__asm__ volatile("mrs %0, primask":"=r"(mask));__disable_irq();
    __asm__ volatile("dsb":::"memory");SCB_MPU_CTRL=0;
    // 96 KiB window: 32 KiB at 0x18000, then 64 KiB at 0x20000.
    for(unsigned i=0;i<2;i++){
        SCB_MPU_RBAR=(i?0x20000u:0x18000u)|SCB_MPU_RBAR_VALID|(11+i);
        SCB_MPU_RASR=SCB_MPU_RASR_TEX(1)|SCB_MPU_RASR_AP(loading?3:7)|
            (loading?SCB_MPU_RASR_XN:0)|SCB_MPU_RASR_SIZE(i?15:14)|SCB_MPU_RASR_ENABLE;
    }
    // Region 14 overrides only the unused top 32 KiB of module ITCM.
    // Keep the same physical banks: data accesses use the CPU TCM interface.
    SCB_MPU_RBAR=VM_AUX_CODE_LIMIT|SCB_MPU_RBAR_VALID|14u;
    SCB_MPU_RASR=auxiliary?(SCB_MPU_RASR_TEX(1)|SCB_MPU_RASR_AP(3)|
        SCB_MPU_RASR_XN|SCB_MPU_RASR_SIZE(14)|SCB_MPU_RASR_ENABLE):0;
    SCB_MPU_CTRL=SCB_MPU_CTRL_ENABLE;__asm__ volatile("dsb\nisb":::"memory");
    if(!mask)__enable_irq();
}
static void constantAccess(bool protect){
    // Region 13: upper six 16 KiB subregions of aligned 128 KiB RAM2 window.
    // Preserve the core's write-back cache attributes; constants are always XN.
    // RAM2 is unused before module loading. Clean loader writes before making RO.
    if(protect)arm_dcache_flush_delete((void *)VM_RAM2_RO_BASE,VM_RAM2_RO_BYTES);
    uint32_t mask;__asm__ volatile("mrs %0, primask":"=r"(mask));__disable_irq();
    __asm__ volatile("dsb":::"memory");SCB_MPU_CTRL=0;
    SCB_MPU_RBAR=0x20260000u|SCB_MPU_RBAR_VALID|13u;
    SCB_MPU_RASR=protect?(SCB_MPU_RASR_TEX(1)|SCB_MPU_RASR_C|SCB_MPU_RASR_B|
        SCB_MPU_RASR_AP(7)|SCB_MPU_RASR_XN|SCB_MPU_RASR_SIZE(16)|(3u<<8)|SCB_MPU_RASR_ENABLE):0;
    SCB_MPU_CTRL=SCB_MPU_CTRL_ENABLE;__asm__ volatile("dsb\nisb":::"memory");
    if(!mask)__enable_irq();
}
static uint32_t timeNow(){return micros();}
static bool indexedVideoUrgent();
static bool shouldYield(){return inputPending||quietRequested||indexedVideoUrgent()||(pending&&EZFlashRAM[0xf6]==sequence)||uint32_t(micros()-sliceStarted)>=1500;}
static bool loadModule(){
    char path[128];snprintf(path,sizeof path,"%s/%s",launch.root,manifest.module);
    FsFile f=SD.sdfs.open(path,O_RDONLY);VmImageHeader h{};
    if(!f||f.isDirectory()||f.fileSize()>UINT32_MAX||f.read(&h,sizeof h)!=sizeof h||!vm_valid_header(h,f.fileSize())||
       (h.required_services&~providedServices)){f.close();failure=0x11;return false;}
    // Bounds/profile/imports are checked before any module memory is written.
    auto code=(uint8_t *)VM_CODE_BASE;auto data=(uint8_t *)VM_DATA_BASE;
    auto ro=(uint8_t *)VM_RAM2_RO_BASE;
    constantAccess(false);
    codeAccess(true);
    const bool loaded=vm_load_payload(h,f,code,data,ro,failure);
    f.close();codeAccess(false,loaded&&h.reserved[0]==VM_PROFILE_RAM1_AUX);
    if(!loaded)return false;
    if(h.reserved[0]==VM_PROFILE_RAM2_RO96)constantAccess(true);
    __asm__ volatile("dsb\nisb":::"memory");
    const uint32_t used=(h.data_bytes+h.bss_bytes+31u)&~31u;
    host={VM_ABI,sizeof(VmHost),providedServices,data+used,VM_DATA_BYTES-used,launch.root,launch.content,timeNow,openFile,readFile,nextFile,closeFile,
        (uint8_t *)VM_RAM_BASE,vm_image_guest_bytes(h),openFlags,writeFile,fileOp,shouldYield,moduleFail};
    host.video_present=videoPresent;
    host.video_configure=configureIndexedVideo;host.video_indexed=submitIndexedVideo;
    if(h.reserved[0]==VM_PROFILE_RAM1_AUX){
        static_assert(sizeof(SwapBuffers)>=16384,"auxiliary swap arena");
        // Boot fixed every decode pointer to RAM_Image before loadModule.
        // No later bank selection or polling may enter the legacy swap path.
        auxiliaryOwned=true;
        host.auxiliary[0]={(uint8_t *)VM_AUX_CODE_LIMIT,32768};
        host.auxiliary[1]={(uint8_t *)SwapBuffers,16384};
    }
    module=reinterpret_cast<VmEntry>(h.entry)(&host);
    // Native modules are trusted, but reject corrupt API pointers before calling.
    const uintptr_t end=VM_CODE_BASE+h.code_bytes;
    auto codePointer=[end](uintptr_t p){return (p&1)&&(p&~1u)>=VM_CODE_BASE&&(p&~1u)<end;};
    const uintptr_t p=(uintptr_t)module;
    if(p<VM_CODE_BASE||p>VM_CODE_LIMIT-sizeof(VmModule)||module->abi!=VM_ABI||module->bytes!=sizeof(VmModule)||
       !codePointer((uintptr_t)module->input)||!codePointer((uintptr_t)module->pump)||!codePointer((uintptr_t)module->packet)||!codePointer((uintptr_t)module->ack)){if(!failure)failure=0x14;module=nullptr;return false;}
    return true;
}
static uint16_t crc16(const uint8_t *p,unsigned n){uint16_t c=0xffff;while(n--){c^=(uint16_t)*p++<<8;for(unsigned b=0;b<8;b++)c=(c<<1)^((c&0x8000)?0x1021:0);}return c;}
#include "VMPacketReplay.h"
static FLASHMEM VmVideoResult videoPresent(const VmVideoFrame *frame){
#if defined(FeatVMVideoDMA) && defined(Fab04_FullDMACapable)
    if(!videoDmaEnabled||(videoTiming&0xfe)!=0x80)return VmVideoResult::Unavailable;
    if(!frame||frame->bytes!=sizeof(VmVideoFrame)||frame->format!=VM_VIDEO_FORMAT_VIC_CELL10||
       (frame->flags&~VM_VIDEO_FLAG_HIRES)||frame->width!=40||frame->height!=25||frame->stride!=10||
       frame->background!=0||frame->reserved||!frame->pixels)return VmVideoResult::Failed;
    const uintptr_t source=(uintptr_t)frame->pixels;
    if(source<VM_DATA_BASE||source>VM_DATA_LIMIT-10000u)return VmVideoResult::Failed;
    if(DMA_State!=DMA_S_DisableReady)return VmVideoResult::Busy;
    const bool ntsc=(videoTiming&1)!=0;
    nS_DMASetup=ntsc?Def_nS_DMASetupNTSC:Def_nS_DMASetupPAL;
    nS_MaxAdj=ntsc?Def_nS_MaxAdjNTSC:Def_nS_MaxAdjPAL;
    uint8_t row[400];bool started=false,okay=true;
    for(uint16_t y=0;y<25&&okay;y++){
        const uint8_t *cells=frame->pixels+y*40u*10u;
        for(uint16_t x=0;x<40;x++){
            memcpy(row+x*8u,cells+x*10u,8);
            row[320+x]=cells[x*10u+8];row[360+x]=cells[x*10u+9]&15;
        }
        const uint16_t cell=y*40u;
        auto segment=[&](uint16_t address,uint8_t *data,uint16_t bytes){
            if(!started){if(!PerformDMA(false,address,data,bytes,false))return false;started=true;return true;}
            return AGIContinueDMA(false,address,data,bytes,false);
        };
        okay=segment(0x6000u+cell*8u,row,320)&&segment(0x5c00u+cell,row+320,40)&&segment(0xd800u+cell,row+360,40);
    }
    const bool closed=started&&CloseDMA();
    if(!okay||!closed){videoDmaEnabled=false;if(DMA_State!=DMA_S_DisableReady)AGIDMAEmergencyRelease();return VmVideoResult::Failed;}
    return VmVideoResult::Transferred;
#else
    (void)frame;return VmVideoResult::Unavailable;
#endif
}
static void fail(uint8_t error){failure=error;EZFlashRAM[0xfb]=error;__asm__ volatile("dmb":::"memory");EZFlashRAM[0xf5]=0xe0;}
static void moduleFail(uint8_t error,uint32_t detail){
    EZFlashRAM[0xf8]=detail;EZFlashRAM[0xf9]=detail>>8;EZFlashRAM[0xfa]=detail>>16;
    fail(error?error:0x16);
}
}
bool VMHostOwnsSwapRAM(){return VmRuntime::auxiliaryOwned;}
// Called only by the stock EasyFlash IO2 handler. No file or VM code in ISR.
#include "VMIndexedVideo.h"
bool VMHostIO2(uint8_t address,bool read){
    using namespace VmRuntime;if(!active||CurrentEasyFlashBank!=58)return false;
    if(read){DataPortWriteWaitLog(EZFlashRAM[address]);return true;}
    const uint8_t value=DataPortWaitRead();TraceLogAddValidData(value);
    if(address==0xf6||(address>=0xf8&&address<=0xfb)||address>=0xfd)EZFlashRAM[address]=value;
    if(address==0xf4){
        EZFlashRAM[address]=value;
        if(value==1&&!started){videoTiming=EZFlashRAM[0xfb];startRequested=true;}
        if(value==4)quietRequested=true;
        if(value==6){quietRequested=true;packetReplayRequested=true;}
        if(value==5)indexedVideoBorder();
        if(value==3&&!inputPending&&EZFlashRAM[0xfe]&&EZFlashRAM[0xfe]!=EZFlashRAM[0xfc]){
            if((uint8_t)(0xa5^EZFlashRAM[0xf8]^EZFlashRAM[0xf9]^EZFlashRAM[0xfa]^EZFlashRAM[0xfd]^EZFlashRAM[0xfe])==EZFlashRAM[0xff]){
                input.buttons=EZFlashRAM[0xf8];input.display=EZFlashRAM[0xf9];input.overflow=EZFlashRAM[0xfa];input.protocol=EZFlashRAM[0xfd];
                __asm__ volatile("dmb":::"memory");inputPending=true;EZFlashRAM[0xfc]=EZFlashRAM[0xfe];
            }
        }
    }return true;
}
#include "VMHostPoll.h"
bool VMHostBoot(){
    using namespace VmRuntime;
    if(!SD.sdfs.begin(SdioConfig(FIFO_SDIO)))return false;
    if(!VmRegistry::consume(launch)||!VmRegistry::readManifest(launch.root,manifest)||manifest.crc!=launch.manifest_crc)return false;
    char path[128];snprintf(path,sizeof path,"%s/%s",launch.root,manifest.client);
    FsFile f=SD.sdfs.open(path,O_RDONLY);uint8_t header[64],chip[16];
    if(!f||f.read(header,64)!=64||memcmp(header,"C64 CARTRIDGE   ",16)||header[23]!=32){f.close();return false;}
    for(unsigned i=0;i<2;i++){
        if(f.read(chip,16)!=16||memcmp(chip,"CHIP",4)||chip[10]||chip[11]||chip[12]!=(i?0xa0:0x80)||chip[13]||chip[14]!=0x20||chip[15]||f.read(RAM_Image+i*8192,8192)!=8192){f.close();return false;}
    }
    uint8_t descriptor[128];
    if(f.read(chip,16)!=16||memcmp(chip,"CHIP",4)||chip[10]||chip[11]!=1||chip[12]!=0x80||chip[13]||
       f.read(descriptor,128)!=128||memcmp(descriptor,"VMH1",4)||descriptor[4]!=VM_ABI||!memchr(descriptor+16,0,24)||
       strcmp((char *)descriptor+16,manifest.id)||vm_crc32(descriptor,124)!=*(uint32_t *)(descriptor+124)||
       vm_crc32(RAM_Image,16384)!=*(uint32_t *)(descriptor+8)){f.close();return false;}
    f.close();
    NumCrtChips=0;memset(EZFlashRAM,0,sizeof EZFlashRAM);CurrentEasyFlashBank=0;
    SetGameAssert;SetExROMDeassert;
    for(unsigned i=0;i<64;i++){BankDecode[i][0]=RAM_Image;BankDecode[i][1]=RAM_Image+8192;}
    LOROM_Image=RAM_Image;HIROM_Image=RAM_Image+8192;LOROM_Mask=HIROM_Mask=8191;
    CurrentIOHandler=IOH_EasyFlash;EmulateVicCycles=false;
    memcpy(EZFlashRAM+0xf0,"M3TP",4);EZFlashRAM[0xf5]=0;
    videoTiming=0;videoDmaEnabled=true;packetReplayRequested=false;active=true;loadModule(); // Failures remain readable by the C64 client.
    doReset=true;return true;
}
