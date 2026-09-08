#define NOMINMAX
#include <windows.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "../abi/vm_abi.h"
#define FLASHMEM
#define FeatVMVideoDMA
#define Fab04_FullDMACapable
enum {DMA_S_DisableReady,DMA_S_Active};
static unsigned DMA_State,nS_DMASetup,nS_MaxAdj;
static uint32_t ARM_DWT_CYCCNT;
static uint32_t clockUs;
static uint32_t micros(){return clockUs;}
static constexpr uint32_t F_CPU_ACTUAL=600000000;
static constexpr unsigned Def_nS_DMASetupNTSC=1,Def_nS_DMASetupPAL=2,Def_nS_MaxAdjNTSC=3,Def_nS_MaxAdjPAL=4;
static uint8_t c64[65536];static unsigned segments;static bool dmaFail;
static bool PerformDMA(bool,uint16_t address,uint8_t *data,uint16_t bytes,bool){
    if(dmaFail)return false;DMA_State=DMA_S_Active;memcpy(c64+address,data,bytes);segments++;return true;
}
static bool AGIContinueDMA(bool r,uint16_t a,uint8_t *d,uint16_t n,bool f){return PerformDMA(r,a,d,n,f);}
static bool CloseDMA(){DMA_State=DMA_S_DisableReady;return true;}
static void AGIDMAEmergencyRelease(){DMA_State=DMA_S_DisableReady;}
namespace VmRuntime {static uint8_t videoTiming=0x80;}
#ifndef MPE_INDEXED_HOST_HEADER
#define MPE_INDEXED_HOST_HEADER "../../Source/Teensy/MinimalBoot/VMIndexedVideo.h"
#endif
#include MPE_INDEXED_HOST_HEADER
using namespace VmRuntime;
int main(){
    void *arena=VirtualAlloc((void *)0x20010000,0x40000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(arena==(void *)0x20010000);
    auto p=(uint8_t *)VM_DATA_BASE;VmIndexedVideoSetup setup{sizeof(setup),p,VM_INDEXED_VIDEO_WORKSPACE_BYTES,0,15,0};
    setup.workspace_bytes--;assert(!configureIndexedVideo(&setup));setup.workspace_bytes++;
    setup.workspace=p+1;assert(!configureIndexedVideo(&setup));setup.workspace=p;assert(configureIndexedVideo(&setup));
    auto pixels=p+VM_INDEXED_VIDEO_WORKSPACE_BYTES;auto palette=pixels+64000;
    const uint8_t rgb[]={0,0,0,255,255,255,136,57,50,103,182,189};memcpy(palette,rgb,sizeof rgb);
    for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++)pixels[y*320+x]=((y&7)>=4?2:0)+(x&1);
    VmIndexedFrame source{sizeof(source),1,pixels,palette,64000,12,320,200,320,4,0};
    auto bad=source;bad.pixel_bytes--;assert(submitIndexedVideo(&bad)==VmVideoResult::Failed);
    bad=source;bad.palette=(uint8_t *)VM_DATA_LIMIT;assert(submitIndexedVideo(&bad)==VmVideoResult::Failed);
    indexedVideo.requested=1;assert(submitIndexedVideo(&source)==VmVideoResult::Busy&&indexedVideo.phase==1);
    const auto frozen=*indexedVideo.frame;indexedVideo.requested=3;
    assert(submitIndexedVideo(&source)==VmVideoResult::Busy&&!memcmp(&frozen,indexedVideo.frame,sizeof frozen));
    VmPacket packet{};assert(indexedVideoPacket(packet)&&packet.type==5&&packet.payload[0]==1&&packet.payload[1]==1);
    indexedVideo.phase=2;assert(transferIndexedVideo());assert(segments==77); // 75 planes, background, timed kernel.
    indexedVideo.phase=3;assert(indexedVideoPacket(packet)&&packet.payload[0]==2);
    indexedVideo.phase=4;assert(submitIndexedVideo(&source)==VmVideoResult::Transferred&&source.resolved_mode==1);
    source.generation++;assert(submitIndexedVideo(&source)==VmVideoResult::Busy);assert(indexedVideo.frame->mode==3&&!indexedVideo.frame->mask);
    indexedVideo.phase=2;assert(transferIndexedVideo());indexedVideo.phase=4;assert(submitIndexedVideo(&source)==VmVideoResult::Transferred);
    const unsigned before=segments;source.generation++;assert(submitIndexedVideo(&source)==VmVideoResult::Transferred&&segments==before+76);
    DMA_State=DMA_S_Active;assert(submitIndexedVideo(&source)==VmVideoResult::Busy);DMA_State=DMA_S_DisableReady;
    // Native NES width stays centered through the actual firmware DMA path.
    source.width=256;source.height=240;source.stride=256;source.pixel_bytes=256*240;
    memset(pixels,1,source.pixel_bytes);source.generation++;
    assert(submitIndexedVideo(&source)==VmVideoResult::Transferred);
    for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++){
        const unsigned cell=(y/8)*40+x/8;
        const auto colors=c64[0x5c00+cell],bits=c64[0x6000+cell*8+y%8];
        const auto color=bits&(0x80>>(x%8))?colors>>4:colors&15;
        assert(color==unsigned(x>=32&&x<288));
    }
    indexedVideo.requested=0;source.generation++;assert(submitIndexedVideo(&source)==VmVideoResult::Busy);
    assert(transferIndexedVideo());indexedVideo.phase=4;assert(submitIndexedVideo(&source)==VmVideoResult::Transferred&&source.resolved_mode==0);
    for(unsigned i=0;i<1000;i++)assert(c64[0xd800+i]<16);
    // Switching to Default repaints the former margins with source content.
    assert(c64[0x6000]==0x55&&c64[0x5c00]==0x10);
    assert(c64[0x6000+39*8]==0x55&&c64[0x5c00+39]==0x10);
    // New clients upload into alternating, inactive banks with bounded grants.
    // An expired grant never starts DMA. No active image byte may change.
    for(uint8_t timing:{0x82,0x83})for(uint8_t mode:{1,2})for(unsigned frame=0;frame<3;frame++){
        videoTiming=timing;indexedVideo.requested=mode;
        for(unsigned y=0;y<240;y++)for(unsigned x=0;x<256;x++)pixels[y*256+x]=frame==2?1:((y%10)>=5?2:0)+(x&1);
        source.generation++;assert(submitIndexedVideo(&source)==VmVideoResult::Busy&&indexedVideo.phase==5);
        const auto previousBank=indexedVideo.activeBank;const auto target=1-previousBank;
        assert(indexedVideo.targetBank==target&&videoBorderWaiting);
        assert(indexedVideoPacket(packet)&&packet.payload[0]==3&&(packet.payload[2]>>1)==target);
        indexedVideoAck();assert(indexedVideo.phase==6);
        static uint8_t visible[16384];memcpy(visible,c64+(previousBank?0x8000:0x4000),sizeof visible);
        unsigned prior=segments;indexedVideoBorder();ARM_DWT_CYCCNT+=F_CPU_ACTUAL/1000;
        assert(transferIndexedVideoSlice()&&segments==prior&&indexedVideo.streamOffset==0);
        unsigned grants=0;
        while(indexedVideo.phase==6){
            const auto beforeOffset=indexedVideo.streamOffset;indexedVideoBorder();
            assert(indexedVideoUrgent()&&transferIndexedVideoSlice()&&!indexedVideoUrgent());
            assert(indexedVideo.streamOffset-beforeOffset<=unsigned((timing&1)?1600:3200));
            assert(!memcmp(visible,c64+(previousBank?0x8000:0x4000),sizeof visible));
            assert(++grants<=9&&indexedVideo.activeBank==previousBank);
        }
        assert(indexedVideo.phase==7&&!videoBorderWaiting);
        for(unsigned cell=0;cell<1000;cell++){
            assert(!memcmp(c64+(target?0xa000:0x6000)+cell*8,indexedVideo.frame->cells[cell],8));
            assert(c64[(target?0x8c00:0x5c00)+cell]==indexedVideo.frame->cells[cell][8]);
            assert(c64[(target?0x8800:0x5800)+cell]==indexedVideo.frame->cells[cell][9]);
        }
        if(indexedVideo.frame->mask)assert(!memcmp(c64+(target?0xc000:0x3000),indexedVideo.kernel,indexedVideo.kernelBytes));
        assert(indexedVideoPacket(packet)&&packet.payload[0]==4);indexedVideoAck();
        assert(indexedVideo.activeBank==target&&submitIndexedVideo(&source)==VmVideoResult::Transferred);
    }
    indexedVideoLegacy();assert(!indexedVideo.displayReady&&!indexedVideo.activeBank);
    source.generation++;assert(submitIndexedVideo(&source)==VmVideoResult::Busy&&indexedVideo.phase==1);
    indexedVideoAck();assert(transferIndexedVideo());indexedVideo.phase=3;indexedVideoAck();
    assert(submitIndexedVideo(&source)==VmVideoResult::Transferred);
    source.generation++;assert(submitIndexedVideo(&source)==VmVideoResult::Busy&&indexedVideo.phase==5);
    indexedVideoAck();indexedVideoBorder();dmaFail=true;
    assert(!transferIndexedVideoSlice()&&DMA_State==DMA_S_DisableReady);dmaFail=false;
    assert(configureIndexedVideo(&setup));indexedVideo.requested=0;
    source.generation++;assert(submitIndexedVideo(&source)==VmVideoResult::Busy);
    indexedVideoAck();assert(transferIndexedVideo());indexedVideo.phase=3;indexedVideoAck();
    assert(submitIndexedVideo(&source)==VmVideoResult::Transferred);
    dmaFail=true;source.generation++;assert(submitIndexedVideo(&source)==VmVideoResult::Failed&&DMA_State==DMA_S_DisableReady);
    // Optional resident-bank cache: compare against the destination bank, not
    // the last displayed frame. Repeated frames need no raster/bitmap traffic.
    dmaFail=false;setup.workspace_bytes=mpe_video::DeltaWorkspaceBytes;
    assert(configureIndexedVideo(&setup));videoTiming=0x83;indexedVideo.requested=2;
    pixels=p+setup.workspace_bytes;palette=pixels+64000;memcpy(palette,rgb,sizeof rgb);
    source.pixels=pixels;source.palette=palette;source.width=256;source.height=240;
    source.stride=256;source.pixel_bytes=256*240;
    unsigned initial=0,changed=0;
    for(unsigned f=0;f<5;f++){
        for(unsigned y=0;y<240;y++)for(unsigned x=0;x<256;x++)pixels[y*256+x]=((y%10)>=5?2:0)+(x&1);
        if(f==4)for(unsigned y=80;y<88;y++)for(unsigned x=100;x<108;x++)pixels[y*256+x]=1;
        source.generation++;assert(submitIndexedVideo(&source)==VmVideoResult::Busy);
        if(indexedVideo.phase==1){indexedVideoAck();assert(transferIndexedVideo());indexedVideo.phase=3;indexedVideoAck();}
        else{
            assert(indexedVideo.phase==5);const unsigned target=indexedVideo.targetBank;
            static uint8_t visible[16384];memcpy(visible,c64+(indexedVideo.activeBank?0x8000:0x4000),sizeof visible);
            indexedVideoAck();unsigned grants=0;
            while(indexedVideo.phase==6){
                const unsigned beforeBytes=indexedVideo.uploadedBytes;indexedVideoBorder();
                assert(transferIndexedVideoSlice());assert(indexedVideo.uploadedBytes-beforeBytes<=1600);
                assert(!memcmp(visible,c64+(indexedVideo.activeBank?0x8000:0x4000),sizeof visible));assert(++grants<12);
            }
            for(unsigned c=0;c<1000;c++){
                assert(!memcmp(c64+(target?0xa000:0x6000)+c*8,indexedVideo.frame->cells[c],8));
                assert(c64[(target?0x8c00:0x5c00)+c]==indexedVideo.frame->cells[c][8]);
                assert(c64[(target?0x8800:0x5800)+c]==indexedVideo.frame->cells[c][9]);
            }
            if(f==1)initial=indexedVideo.uploadedBytes;
            if(f==2||f==3)assert(indexedVideo.uploadedBytes==0&&!indexedVideo.kernelNeeded);
            if(f==4){changed=indexedVideo.uploadedBytes;assert(changed>0&&changed<initial/4);}
            indexedVideoAck();
        }
        assert(submitIndexedVideo(&source)==VmVideoResult::Transferred);
    }
    printf("Delta F5: initial %u bytes, repeated 0 bytes, moving 8x8 patch %u bytes\n",initial,changed);
    indexedVideoLegacy();assert(!indexedVideo.bankValid[0]&&!indexedVideo.bankValid[1]);
    // Generic GB geometry: 160 x 144, wide pixels, 28 black rows each side.
    setup.reserved=VM_INDEXED_NATIVE_HEIGHT|VM_INDEXED_DOUBLE_WIDTH;
    setup.default_mode=0;assert(configureIndexedVideo(&setup));
    const uint8_t gray[]={0,0,0,80,80,80,159,159,159,255,255,255};memcpy(palette,gray,sizeof gray);
    source.width=source.stride=160;source.height=144;source.pixel_bytes=160*144;
    for(unsigned y=0;y<144;y++)for(unsigned x=0;x<160;x++)pixels[y*160+x]=(x+y)%4;
    source.generation++;assert(submitIndexedVideo(&source)==VmVideoResult::Busy);
    indexedVideoAck();assert(transferIndexedVideo());indexedVideo.phase=3;indexedVideoAck();
    assert(submitIndexedVideo(&source)==VmVideoResult::Transferred);
    const unsigned vic[]={0,11,15,1};
    for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++){
        const unsigned cell=(y/8)*40+x/8,code=(c64[0x6000+cell*8+y%8]>>(6-(x%8)/2*2))&3;
        const unsigned colors[]={0,unsigned(c64[0x5c00+cell]>>4),unsigned(c64[0x5c00+cell]&15),unsigned(c64[0xd800+cell])};
        assert(colors[code]==(y>=28&&y<172?vic[((x/2)+(y-28))%4]:0));
    }
    // Raster producers lend live backing only during this synchronous call.
    // Once consumed, neither Busy retries nor DMA may call back into a VM.
    setup.reserved=VM_INDEXED_SEPARATE_SELECTORS;setup.default_mode=2;assert(configureIndexedVideo(&setup));
    struct Raster {uint8_t *pixels;unsigned reads;};auto context=reinterpret_cast<Raster *>(p+setup.workspace_bytes);
    context->pixels=p+setup.workspace_bytes+64;context->reads=0;palette=context->pixels+64000;
    memcpy(palette,rgb,sizeof rgb);memset(context->pixels,1,64000);
    VmIndexedRasterFrame live{};live.frame={sizeof(live),100,nullptr,palette,0,12,320,200,0,4,0};
    live.context=context;live.read_pixel=[](void *p,uint16_t x,uint16_t y){auto &r=*static_cast<Raster *>(p);r.reads++;return r.pixels[y*320+x];};
    auto invalid=live;invalid.read_pixel=nullptr;assert(submitIndexedVideo(&invalid.frame)==VmVideoResult::Failed);
    invalid=live;invalid.context=(void *)VM_DATA_LIMIT;assert(submitIndexedVideo(&invalid.frame)==VmVideoResult::Failed);
    DMA_State=DMA_S_Active;assert(submitIndexedVideo(&live.frame)==VmVideoResult::Busy&&!live.source_consumed&&!context->reads);DMA_State=DMA_S_DisableReady;
    assert(submitIndexedVideo(&live.frame)==VmVideoResult::Busy&&live.source_consumed&&context->reads);
    const auto reads=context->reads;const auto picture=*indexedVideo.frame;memset(context->pixels,2,64000);
    assert(submitIndexedVideo(&live.frame)==VmVideoResult::Busy&&context->reads==reads&&!memcmp(&picture,indexedVideo.frame,sizeof picture));
    invalid=live;invalid.frame.generation++;assert(submitIndexedVideo(&invalid.frame)==VmVideoResult::Failed);
    indexedVideoAck();assert(transferIndexedVideo());indexedVideo.phase=3;indexedVideoAck();
    assert(submitIndexedVideo(&live.frame)==VmVideoResult::Transferred&&context->reads==reads);
    setup.default_mode=0;assert(configureIndexedVideo(&setup));
    live.frame.generation++;live.source_consumed=0;live.geometry=VM_INDEXED_SOURCE_BACKGROUND|VM_INDEXED_RGBI;
    const uint8_t cga[]={0,0,170,0,170,0,170,0,0,170,85,0};memcpy(palette,cga,sizeof cga);memset(context->pixels,0,64000);
    assert(submitIndexedVideo(&live.frame)==VmVideoResult::Busy);
    assert(indexedVideoPacket(packet)&&packet.length==4&&packet.payload[3]==6);
    indexedVideoAck();assert(transferIndexedVideo()&&c64[0xd021]==6);indexedVideo.phase=3;indexedVideoAck();
    assert(submitIndexedVideo(&live.frame)==VmVideoResult::Transferred&&live.resolved_background==6);
    puts("PASS: live raster snapshot, pre-consumption Busy, immutable conversion during live VRAM changes, generation rejection and nonblack background");
#if defined(MPE_LEGACY_FIRMWARE)
    setup.reserved=VM_INDEXED_STABLE_RASTER;assert(!configureIndexedVideo(&setup));
    setup.reserved=0;assert(configureIndexedVideo(&setup));
    puts("PASS: original V1.1.7 firmware rejects the optional hint and accepts the fallback");
#else
    setup.reserved=VM_INDEXED_STABLE_RASTER;setup.default_mode=2;assert(configureIndexedVideo(&setup));
    memcpy(palette,rgb,sizeof rgb);videoTiming=0x83;indexedVideo.displayReady=true;
    VmIndexedFrame fixed{sizeof fixed,200,context->pixels,palette,64000,12,320,200,320,4,0};
    for(unsigned f=0;f<4;f++){
        for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++)context->pixels[y*320+x]=(x+y+f)%4;
        fixed.generation++;assert(submitIndexedVideo(&fixed)==VmVideoResult::Busy&&indexedVideo.phase==5);
        assert(indexedVideo.frame->mask==0x1ffffff);for(auto split:indexedVideo.frame->split)assert(split==4);
        assert(indexedVideo.kernelNeeded==(f<2)); // One program per bank, even as the scene changes.
        indexedVideoAck();while(indexedVideo.phase==6){indexedVideoBorder();assert(transferIndexedVideoSlice());}
        indexedVideoAck();assert(submitIndexedVideo(&fixed)==VmVideoResult::Transferred&&fixed.resolved_mode==2);
    }
    puts("PASS: opt-in stable F5 through actual host, changing pictures reuse both raster programs");
    setup.reserved=1024;assert(!configureIndexedVideo(&setup));
    // Negotiated F5 uses the existing image allocation and ordinary DMA,
    // even with streaming-capable clients. No border grants or FLI kernel.
    setup.reserved=VM_INDEXED_SPRITE_F5|VM_INDEXED_SPRITE_TAGS;
    assert(configureIndexedVideo(&setup));
    fixed.width=fixed.stride=256;fixed.height=240;fixed.pixel_bytes=256*240;
    for(unsigned y=0;y<240;y++)for(unsigned x=0;x<256;x++)context->pixels[y*256+x]=64|((x+y)%4);
    fixed.generation++;assert(submitIndexedVideo(&fixed)==VmVideoResult::Busy&&indexedVideo.phase==1);
    assert(indexedVideo.frame->overlays&&!indexedVideo.frame->mask&&!videoBorderWaiting);
    assert(indexedVideoPacket(packet)&&packet.payload[2]==4);
    const auto spriteFrozen=*indexedVideo.frame;const auto generation=fixed.generation;
    fixed.generation++;assert(submitIndexedVideo(&fixed)==VmVideoResult::Failed);fixed.generation=generation;
    assert(submitIndexedVideo(&fixed)==VmVideoResult::Busy&&!memcmp(&spriteFrozen,indexedVideo.frame,sizeof spriteFrozen));
    indexedVideoAck();assert(transferIndexedVideo());indexedVideo.phase=3;indexedVideoAck();
    assert(submitIndexedVideo(&fixed)==VmVideoResult::Transferred);
    assert(c64[0xd015]&&c64[0xd015]==indexedVideo.frame->cells[529][9]);
    for(unsigned i=0;i<8;i++)assert(c64[0x5ff8+i]==0x60+i);
    for(unsigned i=0;i<17;i++)assert(c64[0xd000+i]==indexedVideo.frame->cells[512+i][9]);
    const unsigned spriteBefore=segments;fixed.generation++;
    assert(submitIndexedVideo(&fixed)==VmVideoResult::Transferred&&segments==spriteBefore); // exact resident repeat: zero DMA
    context->pixels[0]=64|3;fixed.generation++;
    assert(submitIndexedVideo(&fixed)==VmVideoResult::Transferred&&segments==spriteBefore+85);
    assert(!indexedVideoPacket(packet)&&!videoBorderWaiting);
    // Both hires modes retain black 32-pixel NES side margins, including the
    // actual sprite positions; every nonblack overlay stays within source.
    for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++)if(x<32||x>=288){
        const auto c=indexedVideo.frame->cells[y/8*40+x/8];assert(c[8]==0);
        for(unsigned n=0;n<8;n++)if(c64[0xd015]&(1<<n)){
            const int dx=int(x)+24-int(c64[0xd000+n*2]+((c64[0xd010]&(1<<n))?256:0));
            const int dy=int(y)+50-int(c64[0xd001+n*2]);
            if(dx>=0&&dx<24&&dy>=0&&dy<21)assert(!(c64[0x5800+n*64+dy*3+dx/8]&(128>>(dx%8))));
        }
    }
    indexedVideo.requested=1;fixed.generation++;
    assert(submitIndexedVideo(&fixed)==VmVideoResult::Busy&&indexedVideo.phase==1); // never stream sprites into FLI
    indexedVideoAck();assert(transferIndexedVideo());indexedVideo.phase=3;indexedVideoAck();
    assert(submitIndexedVideo(&fixed)==VmVideoResult::Transferred);
    puts("PASS: sprite F5 negotiated, frozen generation, 85-segment ordinary DMA, no raster grants, centered NES margins and timed-mode transition pause");
    setup.reserved=VM_INDEXED_SPRITE_F5|VM_INDEXED_SPRITE_TAGS|VM_INDEXED_CROP_F3;setup.default_mode=0;
    assert(configureIndexedVideo(&setup));indexedVideo.requested=1;indexedVideo.camera.input(1,0,clockUs);
    for(unsigned y=0;y<240;y++)for(unsigned x=0;x<256;x++)context->pixels[y*256+x]=64|((x+y/3+x/11)%4);
    fixed.generation++;assert(submitIndexedVideo(&fixed)==VmVideoResult::Busy&&indexedVideo.phase==1);
    assert(indexedVideo.camera.x==48&&indexedVideo.camera.y==20&&indexedVideo.frame->multicolor&&!indexedVideo.frame->mask&&!videoBorderWaiting);
    const auto cropFrozen=*indexedVideo.frame;assert(indexedVideoPacket(packet)&&packet.payload[1]==1&&packet.payload[2]==8);
    indexedVideo.camera.input(1,8,clockUs);clockUs+=80000;
    assert(submitIndexedVideo(&fixed)==VmVideoResult::Busy&&!memcmp(&cropFrozen,indexedVideo.frame,sizeof cropFrozen));
    assert(indexedVideo.camera.x==48); // held input cannot change a frozen transfer
    const auto cropBefore=segments;indexedVideoAck();assert(transferIndexedVideo());assert(segments==cropBefore+76);
    indexedVideo.phase=3;assert(indexedVideoPacket(packet)&&packet.payload[0]==2&&packet.payload[2]==8);indexedVideoAck();
    assert(submitIndexedVideo(&fixed)==VmVideoResult::Transferred&&fixed.resolved_mode==1);
    const auto checkCrop=[&](){for(unsigned y=0;y<200;y++)for(unsigned x=0;x<160;x++){
        const auto c=y/8*40+x/4;const uint8_t colors[]={c64[0xd021],uint8_t(c64[0x5c00+c]>>4),uint8_t(c64[0x5c00+c]&15),c64[0xd800+c]};
        const auto value=colors[(c64[0x6000+c*8+y%8]>>(6-2*(x%4)))&3];
        assert(value==(context->pixels[(y+indexedVideo.camera.y)*256+x+indexedVideo.camera.x]&63));
    }};checkCrop();
    fixed.generation++;assert(submitIndexedVideo(&fixed)==VmVideoResult::Transferred&&indexedVideo.camera.x>48&&!indexedVideo.phase);checkCrop();
    indexedVideo.camera.input(1,0,clockUs);const auto repeated=segments;fixed.generation++;
    assert(submitIndexedVideo(&fixed)==VmVideoResult::Transferred&&segments==repeated&&!videoBorderWaiting);
    for(uint8_t mode:{2,3,0,1}){
        indexedVideo.camera.input(mode,0,clockUs);indexedVideo.requested=mode;fixed.generation++;
        assert(submitIndexedVideo(&fixed)==VmVideoResult::Busy&&indexedVideo.phase==1&&!videoBorderWaiting);
        indexedVideoAck();assert(transferIndexedVideo());indexedVideo.phase=3;indexedVideoAck();assert(submitIndexedVideo(&fixed)==VmVideoResult::Transferred);
    }
    assert(indexedVideo.camera.x==48&&indexedVideo.camera.y==20);checkCrop();
    puts("PASS: F3 native crop through actual host, ordinary color-RAM DMA, frozen camera during ACK, zero-DMA repeat and F5/F7/F1/F3 transitions");
#endif
    VirtualFree(arena,0,MEM_RELEASE);
    puts("PASS: indexed bounds/lifecycle, centered geometry, legacy restoration, inactive-bank PAL/NTSC sliced uploads, expired grants, atomic ACK ownership, picker reinitialization, DMA failure release");
}
