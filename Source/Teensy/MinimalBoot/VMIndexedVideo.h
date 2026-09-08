// SPDX-License-Identifier: MIT
#pragma once
#define MPE_VIDEO_CODE FLASHMEM
#include "vm/video/mpe_video_live.cpp"
#include "vm/video/mpe_video_kernel.h"
#include "vm/video/mpe_video_camera.h"
#undef MPE_VIDEO_CODE

namespace VmRuntime {
struct IndexedVideoState {
    mpe_video::LiveFrame *frame;
    mpe_video::LiveConverter *converter;
    uint8_t *kernel;
    uint16_t kernelBytes,streamOffset;
    uint8_t phase,requested,preferred,capabilities,activeBank,targetBank;
    uint32_t generation;
    bool configured,hostPacket,displayReady;
    mpe_video::LiveFrame *bank[2];
    bool bankValid[2],kernelNeeded;
    uint16_t uploadedBytes;
    uint16_t geometry;
    mpe_video::CropCamera camera;
};
static IndexedVideoState indexedVideo{};
static volatile bool videoBorderWaiting,videoBorderGrant;
static volatile uint32_t videoBorderCycles;
static bool indexedVideoUrgent(){return videoBorderGrant;}
static void indexedVideoLegacy(){
    // A native CELL replacement takes the display back to VIC bank $4000.
    // The next indexed frame must initialize the receiver again (e.g. picker
    // -> ROM -> picker -> ROM), never assume the old hidden bank is active.
    indexedVideo.displayReady=false;indexedVideo.activeBank=0;
    indexedVideo.bankValid[0]=indexedVideo.bankValid[1]=false;
    videoBorderWaiting=videoBorderGrant=false;
}
static void indexedVideoBorder(){
    if(videoBorderWaiting){videoBorderCycles=ARM_DWT_CYCCNT;videoBorderGrant=true;}
}
static void indexedVideoAck(){
    auto &v=indexedVideo;
    if(v.phase==1)v.phase=2;
    else if(v.phase==5)v.phase=6;
    else {if(v.phase==7)v.activeBank=v.targetBank;else v.activeBank=0;v.phase=4;}
    v.hostPacket=false;
}
static bool transferIndexedVideo();
static_assert(sizeof(mpe_video::LiveFrame)+sizeof(mpe_video::LiveConverter)+mpe_video::KernelCapacity<=VM_INDEXED_VIDEO_WORKSPACE_BYTES,"Indexed workspace overflow");
static_assert(sizeof(mpe_video::LiveFrame)*3+sizeof(mpe_video::LiveConverter)+mpe_video::KernelCapacity<=mpe_video::DeltaWorkspaceBytes,"Delta workspace overflow");
static bool videoRange(const void *p,uint32_t bytes){
    auto address=(uintptr_t)p;return address>=VM_DATA_BASE&&address<=VM_DATA_LIMIT&&bytes<=VM_DATA_LIMIT-address;
}
static FLASHMEM bool configureIndexedVideo(const VmIndexedVideoSetup *setup){
    if(!setup||setup->bytes!=sizeof(*setup)||setup->workspace_bytes<VM_INDEXED_VIDEO_WORKSPACE_BYTES||
       ((uintptr_t)setup->workspace&3)||!videoRange(setup->workspace,VM_INDEXED_VIDEO_WORKSPACE_BYTES)||
       setup->default_mode>3||(setup->capabilities&~15)||!(setup->capabilities&(1u<<setup->default_mode))||(setup->reserved&~1023))return false;
    indexedVideo={};videoBorderWaiting=videoBorderGrant=false;auto p=(uint8_t *)setup->workspace;memset(p,0,VM_INDEXED_VIDEO_WORKSPACE_BYTES);
    indexedVideo.frame=(mpe_video::LiveFrame *)p;p+=sizeof(mpe_video::LiveFrame);
    indexedVideo.converter=(mpe_video::LiveConverter *)p;p+=sizeof(mpe_video::LiveConverter);
    indexedVideo.kernel=p;indexedVideo.requested=indexedVideo.preferred=setup->default_mode;
    p+=mpe_video::KernelCapacity;
    if(setup->workspace_bytes>=mpe_video::DeltaWorkspaceBytes&&videoRange(setup->workspace,mpe_video::DeltaWorkspaceBytes)){
        indexedVideo.bank[0]=(mpe_video::LiveFrame *)p;p+=sizeof(mpe_video::LiveFrame);
        indexedVideo.bank[1]=(mpe_video::LiveFrame *)p;
    }
    indexedVideo.geometry=setup->reserved;indexedVideo.capabilities=setup->capabilities;indexedVideo.configured=true;return true;
}
static FLASHMEM VmVideoResult submitIndexedVideo(VmIndexedFrame *source){
    auto &v=indexedVideo;
    if(!v.configured||!source)return VmVideoResult::Unavailable;
    const bool raster=source->bytes==sizeof(VmIndexedRasterFrame);
    if(!raster&&source->bytes!=sizeof(*source))return VmVideoResult::Unavailable;
    auto reader=raster?reinterpret_cast<VmIndexedRasterFrame *>(source):nullptr;
    if(v.phase==4){
        if(source->generation!=v.generation)return VmVideoResult::Failed;
        source->resolved_mode=v.frame->mode;if(reader)reader->resolved_background=v.frame->background;
        v.phase=0;v.displayReady=true;return VmVideoResult::Transferred;
    }
    if(v.phase)return source->generation==v.generation?VmVideoResult::Busy:VmVideoResult::Failed;
    if(!source->width||!source->height||source->width>1024||source->height>1024||
       !source->colors||source->colors>256||source->palette_bytes<uint32_t(source->colors)*3||
       !videoRange(source->palette,source->palette_bytes))return VmVideoResult::Failed;
    if(reader){
        if(source->pixels||source->pixel_bytes||!reader->read_pixel||reader->reserved||(reader->geometry&~59)||
           !videoRange(reader->context,1))return VmVideoResult::Failed;
#if defined(__arm__)
        const uintptr_t callback=reinterpret_cast<uintptr_t>(reader->read_pixel);
        if(!(callback&1)||(callback&~uintptr_t(1))<VM_CODE_BASE||(callback&~uintptr_t(1))>=VM_CODE_LIMIT)return VmVideoResult::Failed;
#endif
    }else if(source->stride<source->width||source->pixel_bytes<uint32_t(source->stride)*source->height||
             !videoRange(source->pixels,source->pixel_bytes))return VmVideoResult::Failed;
    if(DMA_State!=DMA_S_DisableReady)return VmVideoResult::Busy;
    const bool sprites=v.requested==2&&(v.geometry&VM_INDEXED_SPRITE_F5);
    const bool crop=v.requested==1&&(v.geometry&VM_INDEXED_CROP_F3);
    const bool direct=v.displayReady&&!v.activeBank&&!v.frame->mask&&v.frame->mode==v.requested&&(v.requested==0||v.requested==3||sprites||crop);
    // Sprite F5 uses the ordinary held-DMA path. A transition to/from a timed
    // mode pauses at the border, clearing sprites before any FLI kernel runs.
    const bool streaming=v.displayReady&&!v.frame->overlays&&!v.frame->multicolor&&!sprites&&!crop&&(videoTiming&2)&&(v.requested==1||v.requested==2);
    mpe_video::IndexedSource s{source->pixels,source->palette,source->width,source->height,source->stride,source->colors,
        uint16_t((reader?reader->geometry:v.geometry&59)|(v.geometry&(VM_INDEXED_STABLE_RASTER|VM_INDEXED_SPRITE_F5|VM_INDEXED_SPRITE_TAGS|VM_INDEXED_CROP_F3))),reader?reader->read_pixel:nullptr,reader?reader->context:nullptr};
    if(crop){v.camera.position(source->width,source->height,micros());s.crop_x=v.camera.x;s.crop_y=v.camera.y;}
    if(!v.converter->render(s,v.requested,*v.frame,v.frame))return VmVideoResult::Failed;
    if(reader)reader->source_consumed=1;
    // Plain Color/Sharp retain the speed candidate's single held-DMA path.
    // Only mode transitions and timed raster kernels require pause/resume.
    if(direct){
        const bool unchanged=(sprites||crop)&&v.bank[0]&&v.bankValid[0]&&v.frame->background==v.bank[0]->background&&
            !memcmp(v.frame->cells,v.bank[0]->cells,sizeof v.frame->cells);
        if(!unchanged&&!transferIndexedVideo())return VmVideoResult::Failed;
        source->resolved_mode=v.frame->mode;if(reader)reader->resolved_background=v.frame->background;
        return VmVideoResult::Transferred;
    }
    v.targetBank=streaming?1-v.activeBank:0;
    const auto old=(v.bank[v.targetBank]&&v.bankValid[v.targetBank])?v.bank[v.targetBank]:nullptr;
    v.kernelNeeded=v.frame->mask&&(!old||old->mask!=v.frame->mask||memcmp(old->split,v.frame->split,25));
    v.kernelBytes=mpe_video::buildKernel(*v.frame,(videoTiming&1)!=0,v.kernel,mpe_video::KernelCapacity,v.targetBank?0xc000:0x3000);
    if(!v.kernelBytes)return VmVideoResult::Failed;
    v.generation=source->generation;v.streamOffset=0;v.uploadedBytes=0;v.phase=streaming?5:1;
    videoBorderGrant=false;videoBorderWaiting=streaming;return VmVideoResult::Busy;
}
static FLASHMEM bool indexedVideoPacket(VmPacket &packet){
    auto &v=indexedVideo;if(v.phase!=1&&v.phase!=3&&v.phase!=5&&v.phase!=7)return false;
    packet={};packet.type=5;packet.length=3;
    packet.payload[0]=v.phase==1?1:v.phase==3?2:v.phase==5?3:4;
    packet.payload[1]=v.frame->mode;packet.payload[2]=(v.frame->mask!=0)|(v.frame->overlays?4:0)|(v.frame->multicolor?8:0)|((v.phase==5||v.phase==7)?v.targetBank<<1:0);
    if(v.frame->background){packet.length=4;packet.payload[3]=v.frame->background;}
    return true;
}
static FLASHMEM bool transferIndexedVideo(){
#if defined(FeatVMVideoDMA) && defined(Fab04_FullDMACapable)
    auto &v=indexedVideo;if((videoTiming&0xfc)!=0x80)return false;
    nS_DMASetup=(videoTiming&1)?Def_nS_DMASetupNTSC:Def_nS_DMASetupPAL;
    nS_MaxAdj=(videoTiming&1)?Def_nS_MaxAdjNTSC:Def_nS_MaxAdjPAL;
    uint8_t row[400];bool started=false,okay=true;
    auto segment=[&](uint16_t address,uint8_t *data,uint16_t bytes){
        if(!started){if(!PerformDMA(false,address,data,bytes,false))return false;started=true;return true;}
        return AGIContinueDMA(false,address,data,bytes,false);
    };
    // Disable sprites while replacing their patterns. Only negotiated sprite
    // clients use this plane; legacy modules retain their original traffic.
    uint8_t zero=0;
    if(v.frame->overlays)okay=segment(0xd015,&zero,1);
    for(unsigned y=0;y<25&&okay;y++){
        for(unsigned x=0;x<40;x++){auto cell=v.frame->cells[y*40+x];memcpy(row+x*8,cell,8);row[320+x]=cell[8];row[360+x]=cell[9];}
        okay=segment(0x6000+y*320,row,320)&&segment(0x5c00+y*40,row+320,40)&&
            segment(v.frame->mode==0||v.frame->multicolor?0xd800+y*40:0x5800+y*40,row+360,40);
    }
    if(okay)okay=segment(0xd021,&v.frame->background,1);
    if(okay&&v.frame->overlays){
        for(unsigned i=0;i<17;i++)row[i]=v.frame->cells[512+i][9];
        okay=segment(0xd000,row,17);
        for(unsigned i=0;i<8;i++){row[i]=v.frame->cells[530+i][9];row[8+i]=0x60+i;}
        if(okay)okay=segment(0xd027,row,8)&&segment(0x5ff8,row+8,8)&&
            segment(0xd017,&zero,1)&&segment(0xd01b,&zero,1)&&segment(0xd01c,&zero,1)&&segment(0xd01d,&zero,1);
        if(okay)okay=segment(0xd015,&v.frame->cells[529][9],1);
    }
    if(okay&&v.frame->mask)okay=segment(0x3000,v.kernel,v.kernelBytes);
    bool closed=started&&CloseDMA();if(!okay||!closed){if(DMA_State!=DMA_S_DisableReady)AGIDMAEmergencyRelease();return false;}
    if(v.bank[0]){*v.bank[0]=*v.frame;v.bankValid[0]=true;}
    return true;
#else
    return false;
#endif
}
// Upload only to the inactive bank, in bounded vertical-border grants. No
// DEN clear and no stopping the raster kernel during a visible scanline.
static FLASHMEM bool transferIndexedVideoSlice(){
#if defined(FeatVMVideoDMA) && defined(Fab04_FullDMACapable)
    auto &v=indexedVideo;if(!videoBorderGrant)return true;
    const uint32_t stamp=videoBorderCycles;videoBorderGrant=false;
    // Grant is emitted just after visible line 250. Reject late service;
    // C64 retries next frame without losing the visible image or source data.
    if(uint32_t(ARM_DWT_CYCCNT-stamp)>F_CPU_ACTUAL/1000000u*500u)return true;
    nS_DMASetup=(videoTiming&1)?Def_nS_DMASetupNTSC:Def_nS_DMASetupPAL;
    nS_MaxAdj=(videoTiming&1)?Def_nS_MaxAdjNTSC:Def_nS_MaxAdjPAL;
    const unsigned total=10000+(v.kernelNeeded?v.kernelBytes:0);
    const auto old=(v.bank[v.targetBank]&&v.bankValid[v.targetBank])?v.bank[v.targetBank]:nullptr;
    auto byteAt=[](const mpe_video::LiveFrame *f,unsigned offset)->uint8_t{
        return offset<8000?f->cells[offset/8][offset%8]:f->cells[(offset-8000)%1000][offset<9000?8:9];
    };
    unsigned budget=(videoTiming&1)?1600:3200;
    uint8_t data[400];bool started=false,okay=true;
    while(budget&&v.streamOffset<total&&okay){
        // Compare against the actual destination bank (two pictures back),
        // never just the currently visible image. Exact bytes, no hash misses.
        if(old)while(v.streamOffset<10000&&byteAt(old,v.streamOffset)==byteAt(v.frame,v.streamOffset))++v.streamOffset;
        if(v.streamOffset==total)break;
        // Also bound wall time, not just bytes: stop between segments if bus
        // acquisition or preparation was unexpectedly slow. The next border
        // resumes at the last complete segment. Leave room for one 400-byte
        // segment plus release well before the next line-48 raster IRQ.
        if(uint32_t(ARM_DWT_CYCCNT-stamp)>F_CPU_ACTUAL/1000000u*((videoTiming&1)?2300u:4300u))break;
        const unsigned offset=v.streamOffset;uint16_t address;unsigned count;
        if(offset<8000){
            count=8000-offset;if(count>sizeof data)count=sizeof data;
            address=(v.targetBank?0xa000:0x6000)+offset;
            for(unsigned i=0;i<count;i++)data[i]=v.frame->cells[(offset+i)/8][(offset+i)%8];
        }else if(offset<10000){
            const bool lower=offset>=9000;const unsigned cell=offset-(lower?9000:8000);
            count=1000-cell;if(count>sizeof data)count=sizeof data;
            address=(v.targetBank?0x3000:0)+(lower?0x5800:0x5c00)+cell;
            for(unsigned i=0;i<count;i++)data[i]=v.frame->cells[cell+i][lower?9:8];
        }else{
            count=total-offset;if(count>sizeof data)count=sizeof data;
            address=(v.targetBank?0xc000:0x3000)+offset-10000;
            memcpy(data,v.kernel+offset-10000,count);
        }
        if(count>budget)count=budget;
        // Stop before a long unchanged run. Short equal gaps are cheaper to
        // send than to repeatedly release/reacquire a DMA segment.
        if(old&&offset<10000){
            unsigned last=0,gap=0;
            for(unsigned i=0;i<count;i++){
                if(byteAt(old,offset+i)!=data[i]){last=i+1;gap=0;}
                else if(++gap>=16)break;
            }
            if(last)count=last;
        }
        okay=started?AGIContinueDMA(false,address,data,count,false):PerformDMA(false,address,data,count,false);
        started=true;if(okay){v.streamOffset+=count;budget-=count;v.uploadedBytes+=count;}
    }
    const bool closed=!started||CloseDMA();
    if(!okay||!closed){if(DMA_State!=DMA_S_DisableReady)AGIDMAEmergencyRelease();return false;}
    if(v.streamOffset==total){
        if(v.bank[v.targetBank]){*v.bank[v.targetBank]=*v.frame;v.bankValid[v.targetBank]=true;}
        videoBorderWaiting=false;videoBorderGrant=false;v.phase=7;
    }
    return true;
#else
    return false;
#endif
}
}
