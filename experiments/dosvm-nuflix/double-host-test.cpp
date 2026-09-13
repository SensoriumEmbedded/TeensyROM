#define NOMINMAX
#include <windows.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <array>
#include <vector>
#include <chrono>
#include "../../vm/abi/vm_abi.h"
#define FLASHMEM
#define FeatVMVideoDMA
#define Fab04_FullDMACapable
#define MPE_DOS_NUFLIX
#define MPE_DOS_NUFLIX_DOUBLE
enum {DMA_S_DisableReady,DMA_S_Active};
static unsigned DMA_State,nS_DMASetup,nS_MaxAdj,segments,failSegment,emergencyReleases;
static uint32_t ARM_DWT_CYCCNT,clockUs;
static uint32_t micros(){return clockUs;}
static constexpr uint32_t F_CPU_ACTUAL=600000000;
static constexpr unsigned Def_nS_DMASetupNTSC=1,Def_nS_DMASetupPAL=2,Def_nS_MaxAdjNTSC=3,Def_nS_MaxAdjPAL=4;
static uint8_t c64[65536];static bool ramOpen,closeFailure;
static bool PerformDMA(bool read,uint16_t address,uint8_t *data,uint16_t bytes,bool){
    ++segments;DMA_State=DMA_S_Active;
    if(segments==failSegment)return false;
    assert(unsigned(address)+bytes<=65536);
    if(read)memcpy(data,c64+address,bytes);
    else{assert(ramOpen||address<0xd000||address>=0xe000);memcpy(c64+address,data,bytes);}
    ARM_DWT_CYCCNT+=(bytes+2)*600;clockUs+=bytes+2;return true;
}
static bool AGIContinueDMA(bool read,uint16_t address,uint8_t *data,uint16_t bytes,bool fixed){return PerformDMA(read,address,data,bytes,fixed);}
static bool CloseDMA(){if(closeFailure)return false;DMA_State=DMA_S_DisableReady;return true;}
static void AGIDMAEmergencyRelease(){++emergencyReleases;DMA_State=DMA_S_DisableReady;}
namespace VmRuntime {static uint8_t videoTiming=0x80;}
#include "../../Source/Teensy/MinimalBoot/VMIndexedVideo.h"
using namespace VmRuntime;
static uint8_t readPixel(void *context,uint16_t column,uint16_t row){return static_cast<const uint8_t *>(context)[row*320+column];}
static void packet(unsigned operation){VmPacket packet{};assert(indexedVideoPacket(packet)&&packet.type==5&&packet.payload[0]==operation);indexedVideoAck();}
int main(int argc,char **argv){
    assert(argc>=4);
    assert(VirtualAlloc(reinterpret_cast<void *>(0x20010000),0x40000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)==reinterpret_cast<void *>(0x20010000));
    auto arena=reinterpret_cast<void *>(VM_DATA_BASE);auto pixels=static_cast<uint8_t *>(arena)+24000,palette=pixels+64000,dirty=palette+48;
    const uint8_t rgb[48]={0,0,0,255,255,255,136,57,50,103,182,189,139,63,150,85,160,73,64,49,141,191,206,114,139,84,41,87,66,0,184,105,98,80,80,80,120,120,120,148,224,137,120,105,196,159,159,159};
    memcpy(palette,rgb,48);memset(dirty,255,125);
    VmCenterVideoSetup setup{{sizeof(setup),arena,VM_NUFLIX_VIDEO_WORKSPACE_BYTES,2,4,VM_INDEXED_NUFLIX_F5|VM_INDEXED_SEPARATE_SELECTORS},0,25,0};
    VmIndexedDirtyRasterFrame source{};source.raster.frame={sizeof(source),1,nullptr,palette,0,48,320,200,0,16,0};source.raster.context=pixels;source.raster.read_pixel=readPixel;source.source_dirty=dirty;
    printf("standard,input,bank,host_ms,payload_bytes,grants,host_pipeline_ms\n");
    for(unsigned standard=0;standard<2;++standard){
        videoTiming=0x8a;assert(!configureIndexedVideo(&setup.setup));videoTiming=standard?0x8f:0x8e;assert(configureIndexedVideo(&setup.setup));memset(c64,0xa5,sizeof(c64));
        auto &state=*static_cast<NuflixState *>(indexedVideo.nuflix);
        for(int argument=2;argument<argc;++argument){
            FILE *file=fopen(argv[argument],"rb");assert(file&&fread(pixels,1,64000,file)==64000);fclose(file);++source.raster.frame.generation;
            const auto before=c64;std::array<uint8_t,65536> saved;memcpy(saved.data(),before,65536);
            const unsigned active=indexedVideo.activeBank;const bool visible=indexedVideo.displayReady;
            const auto start=std::chrono::steady_clock::now();const auto result=submitIndexedVideo(&source.raster.frame);
            const double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            unsigned grants=0;
            if(result==VmVideoResult::Busy){
                assert(source.raster.source_consumed==1&&!configureIndexedVideo(nullptr));
                const auto frozen=state.display;auto wrong=source;++wrong.raster.frame.generation;assert(submitIndexedVideo(&wrong.raster.frame)==VmVideoResult::Failed);
                assert(submitIndexedVideo(&source.raster.frame)==VmVideoResult::Busy&&!memcmp(&frozen,&state.display,sizeof(frozen)));
                if(visible){
                    assert(indexedVideo.phase==5&&indexedVideo.targetBank!=active);packet(3);
                    const unsigned count=segments;assert(transferIndexedVideoSlice()&&segments==count);
                    indexedVideoBorder();ARM_DWT_CYCCNT+=600*251;assert(transferIndexedVideoSlice()&&segments==count);
                    c64[0x06a0]=0;indexedVideoBorder();assert(transferIndexedVideoSlice()&&segments==count+1&&DMA_State==DMA_S_DisableReady);
                    assert(!memcmp(saved.data()+0x1000+active*0x8000,c64+0x1000+active*0x8000,27136));
                    while(indexedVideo.phase==6){
                        ramOpen=true;c64[0x06a0]=0xa5;c64[0x06a1]=0;indexedVideoBorder();const unsigned uploaded=state.uploadedBytes;
                        assert(transferIndexedVideoSlice()&&c64[0x06a1]==1&&DMA_State==DMA_S_DisableReady);
                        assert(state.uploadedBytes-uploaded<=(standard?1536:3072));
                        assert(!memcmp(saved.data()+0x1000+active*0x8000,c64+0x1000+active*0x8000,27136));
                        ramOpen=false;c64[0x06a0]=0;++grants;assert(grants<=20);
                    }
                    assert(indexedVideo.phase==7);packet(4);
                }else{assert(indexedVideo.phase==1);packet(1);assert(transferIndexedVideo());indexedVideo.phase=3;packet(2);}
                assert(submitIndexedVideo(&source.raster.frame)==VmVideoResult::Transferred);
            }else assert(result==VmVideoResult::Transferred);
            const double pipeline=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            const unsigned bank=indexedVideo.activeBank;
            for(unsigned offset=0;offset<27136;++offset)assert(c64[0x1000+bank*0x8000+offset]==dosvm_nuflix::bankDisplayByte(state.display,0x1000+offset,bank,state.mirrorOperand));
            for(unsigned address=0;address<65536;++address)if(!(address>=0x1000&&address<0x7a00)&&!(address>=0x9000&&address<0xfa00)&&address!=0x06a0&&address!=0x06a1)assert(c64[address]==0xa5);
            const std::string output=std::string(argv[1])+"/double-host-"+std::to_string(argument-2)+(standard?"-ntsc":"-pal")+".prg";
            file=fopen(output.c_str(),"wb");assert(file);fputc(0,file);fputc(bank?0x90:0x10,file);fwrite(c64+0x1000+bank*0x8000,1,27136,file);fclose(file);
            printf("%s,%d,%u,%.3f,%u,%u,%.3f\n",standard?"ntsc":"pal",argument-2,bank,elapsed,result==VmVideoResult::Busy?state.uploadedBytes:0,grants,pipeline);
            const unsigned count=segments;++source.raster.frame.generation;assert(submitIndexedVideo(&source.raster.frame)==VmVideoResult::Transferred&&segments==count&&indexedVideo.activeBank==bank);
        }
        pixels[0]^=1;++source.raster.frame.generation;assert(submitIndexedVideo(&source.raster.frame)==VmVideoResult::Busy&&indexedVideo.phase==5);packet(3);
        std::array<uint8_t,27136> saved;memcpy(saved.data(),c64+0x1000+indexedVideo.activeBank*0x8000,27136);
        ramOpen=true;c64[0x06a0]=0xa5;failSegment=segments+3;indexedVideoBorder();assert(!transferIndexedVideoSlice());failSegment=0;ramOpen=false;
        assert(!state.valid&&!indexedVideo.bankValid[indexedVideo.targetBank]&&DMA_State==DMA_S_DisableReady);
        assert(!memcmp(saved.data(),c64+0x1000+indexedVideo.activeBank*0x8000,27136));
        const unsigned releases=emergencyReleases;
        state.valid=true;indexedVideo.bankValid[indexedVideo.targetBank]=true;c64[0x06a0]=0;closeFailure=true;indexedVideoBorder();assert(!transferIndexedVideoSlice());closeFailure=false;
        assert(!state.valid&&!indexedVideo.bankValid[indexedVideo.targetBank]&&DMA_State==DMA_S_DisableReady&&emergencyReleases==releases+1);
        assert(!memcmp(saved.data(),c64+0x1000+indexedVideo.activeBank*0x8000,27136));
        indexedVideo.phase=0;assert(configureIndexedVideo(nullptr));
    }
    printf("PASS: double-buffer host, two independent bank histories, every active byte preserved throughout upload, early/late grant rejection, under-I/O guard, per-grant payload bound, immutable source and failure release; workspace %u bytes; host DMA is simulated\n",unsigned(sizeof(NuflixState)));
}
