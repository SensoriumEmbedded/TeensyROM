#if defined(MPE_DOS_NUFLIX_DOUBLE)
#include "double-host.h"
#else
struct NuflixState {
    dosvm_nuflix::NativeDisplay display;
    uint8_t dirty[106],palette[48],map[16];
    uint16_t width,height,geometry;
    uint32_t conversionUs,transferUs,uploadedBytes;
    uint16_t fitRegions;
    uint8_t unchangedBands,packedBands,colorCount;
    bool valid;
};
static_assert(sizeof(NuflixState)<=VM_NUFLIX_VIDEO_WORKSPACE_BYTES,"Live NUFLIX exceeds DOS workspace loan");
static FLASHMEM uint32_t nuflixChunkHash(const dosvm_nuflix::NativeDisplay &display,unsigned chunk){
    uint32_t hash=2166136261u;
    for(unsigned offset=0;offset<32;++offset)hash=(hash^dosvm_nuflix::displayByte(display,0x1000+chunk*32+offset))*16777619u;
    return hash;
}
static bool nuflixCriticalChunk(unsigned chunk){const unsigned page=chunk/8;return page<17||page==18||(page>=32&&page<36);}
static FLASHMEM bool configureNuflix(const VmIndexedVideoSetup *setup){
    if(indexedVideo.phase||setup->bytes!=sizeof(VmCenterVideoSetup)||
       setup->workspace_bytes<VM_NUFLIX_VIDEO_WORKSPACE_BYTES||((uintptr_t)setup->workspace&3)||
       !videoRange(setup->workspace,VM_NUFLIX_VIDEO_WORKSPACE_BYTES)||setup->default_mode!=2||setup->capabilities!=4||
       ((videoTiming&0xfc)!=0x84&&(videoTiming&0xfc)!=0x88))return false;
    const auto &center=*reinterpret_cast<const VmCenterVideoSetup *>(setup);
    if(center.first_row||center.row_count!=25||center.reserved)return false;
    indexedVideo={};videoBorderWaiting=videoBorderGrant=false;
    indexedVideo.nuflix=setup->workspace;memset(setup->workspace,0,sizeof(NuflixState));
    indexedVideo.geometry=setup->reserved;indexedVideo.requested=indexedVideo.preferred=2;
    indexedVideo.capabilities=4;indexedVideo.configured=true;return true;
}
struct NuflixSource {
    VmIndexedRasterFrame *raster;
    const uint8_t *map;
};
static FLASHMEM uint8_t readNuflixPixel(const void *context,unsigned column,unsigned row){
    const auto &source=*static_cast<const NuflixSource *>(context);const auto &frame=source.raster->frame;
    unsigned horizontal=column*frame.width/320,vertical=row*frame.height/200;
    return source.map[source.raster->read_pixel(source.raster->context,horizontal,vertical)&15];
}
static FLASHMEM VmVideoResult submitNuflix(VmIndexedFrame *source){
    auto &video=indexedVideo;
    if(!source||source->bytes!=sizeof(VmIndexedDirtyRasterFrame))return VmVideoResult::Failed;
    auto &raster=reinterpret_cast<VmIndexedDirtyRasterFrame *>(source)->raster;
    if(video.phase==4){
        if(source->generation!=video.generation)return VmVideoResult::Failed;
        video.phase=0;video.displayReady=true;source->resolved_mode=2;raster.resolved_background=0;return VmVideoResult::Transferred;
    }
    if(video.phase)return source->generation==video.generation?VmVideoResult::Busy:VmVideoResult::Failed;
    if(!source->width||source->width>640||source->height!=200||!source->colors||source->colors>16||source->palette_bytes!=48||
       !videoSourceRange(source->palette,48)||source->pixels||source->pixel_bytes||!raster.read_pixel||raster.reserved||
       (raster.geometry&~59)||!videoSourceRange(raster.context,1))return VmVideoResult::Failed;
    const auto dirty=reinterpret_cast<VmIndexedDirtyRasterFrame *>(source)->source_dirty;
    if(dirty&&!videoSourceRange(dirty,125))return VmVideoResult::Failed;
#if defined(__arm__)
    const uintptr_t callback=reinterpret_cast<uintptr_t>(raster.read_pixel);
    if(!(callback&1)||(callback&~uintptr_t(1))<VM_CODE_BASE||(callback&~uintptr_t(1))>=VM_AUX_CODE_LIMIT)return VmVideoResult::Failed;
#endif
    if(DMA_State!=DMA_S_DisableReady)return VmVideoResult::Busy;
    auto &state=*static_cast<NuflixState *>(video.nuflix);
    const bool previous=state.valid&&video.displayReady;
    const bool same=previous&&state.width==source->width&&state.height==source->height&&state.colorCount==source->colors&&state.geometry==raster.geometry&&!memcmp(state.palette,source->palette,48);
    uint8_t changed=0;if(dirty)for(unsigned index=0;index<125;++index)changed|=dirty[index];
    if(same&&dirty&&!changed){raster.source_consumed=1;source->resolved_mode=2;raster.resolved_background=0;return VmVideoResult::Transferred;}
    const uint32_t started=micros();
    uint32_t hashes[848];
    if(previous)for(unsigned chunk=0;chunk<848;++chunk)hashes[chunk]=nuflixChunkHash(state.display,chunk);
    state.valid=false;
    static constexpr uint8_t rgb[16][3]={{0,0,0},{255,255,255},{136,57,50},{103,182,189},{139,63,150},{85,160,73},{64,49,141},{191,206,114},{139,84,41},{87,66,0},{184,105,98},{80,80,80},{120,120,120},{148,224,137},{120,105,196},{159,159,159}};
    memset(state.map,0,sizeof(state.map));
    for(unsigned color=0;color<source->colors;++color){
        unsigned minimum=UINT32_MAX,selected=0;
        for(unsigned candidate=0;candidate<16;++candidate){
            unsigned distance=0;
            for(unsigned channel=0;channel<3;++channel){int delta=int(source->palette[color*3+channel])-rgb[candidate][channel];distance+=delta*delta;}
            if(distance<minimum){minimum=distance;selected=candidate;}
        }
        state.map[color]=selected;
    }
    NuflixSource pixels{&raster,state.map};dosvm_nuflix::NativeSource reader{&pixels,readNuflixPixel};
    dosvm_nuflix::NativeConverter colors;
    const auto cached=dosvm_nuflix::CachedConverter::convert(colors,reader,state.display,(videoTiming&1)!=0,dosvm_nuflix::displayTemplate+0x2140,dirty,same);
    if(!cached.success)return VmVideoResult::Failed;
    state.fitRegions=uint16_t(cached.fit.regions);state.unchangedBands=uint8_t(cached.unchangedBands);state.packedBands=uint8_t(cached.packedBands);
    memset(state.dirty,0,sizeof(state.dirty));
    bool any=false,critical=false;unsigned uploadBytes=0;
    for(unsigned chunk=0;chunk<848;++chunk){
        if(!previous||nuflixChunkHash(state.display,chunk)!=hashes[chunk]){
            state.dirty[chunk/8]|=1u<<(chunk&7);any=true;uploadBytes+=32;
            if(nuflixCriticalChunk(chunk))critical=true;
        }
    }
    state.width=source->width;state.height=source->height;state.geometry=raster.geometry;state.colorCount=uint8_t(source->colors);memcpy(state.palette,source->palette,48);
    state.conversionUs=micros()-started;
    state.valid=true;raster.source_consumed=1;raster.resolved_background=0;source->resolved_mode=2;
    if(!any)return VmVideoResult::Transferred;
    const bool stream=same&&!critical&&(videoTiming&0xfc)==0x88&&uploadBytes<=((videoTiming&1)?1600u:3200u);
    video.generation=source->generation;video.phase=stream?5:1;video.streamOffset=0;
    videoBorderWaiting=stream;videoBorderGrant=false;state.transferUs=state.uploadedBytes=0;
    return VmVideoResult::Busy;
}
static FLASHMEM bool transferNuflix(){
#if defined(FeatVMVideoDMA) && defined(Fab04_FullDMACapable)
    if((videoTiming&0xfc)!=0x84&&(videoTiming&0xfc)!=0x88)return false;
    auto &state=*static_cast<NuflixState *>(indexedVideo.nuflix);
    nS_DMASetup=(videoTiming&1)?Def_nS_DMASetupNTSC:Def_nS_DMASetupPAL;
    nS_MaxAdj=(videoTiming&1)?Def_nS_MaxAdjNTSC:Def_nS_MaxAdjPAL;
    uint8_t bytes[256];bool started=false,okay=true;state.uploadedBytes=0;const uint32_t stamp=micros();
    for(unsigned chunk=0;chunk<848&&okay;){
        if(!(state.dirty[chunk/8]&(1u<<(chunk&7)))){++chunk;continue;}
        const unsigned first=chunk;
        do{++chunk;}while(chunk<848&&chunk-first<8&&(state.dirty[chunk/8]&(1u<<(chunk&7))));
        const unsigned address=0x1000+first*32,length=(chunk-first)*32;
        for(unsigned offset=0;offset<length;++offset)bytes[offset]=dosvm_nuflix::displayByte(state.display,address+offset);
        okay=started?AGIContinueDMA(false,address,bytes,length,false):PerformDMA(false,address,bytes,length,false);
        if(okay){started=true;state.uploadedBytes+=length;}
    }
    bool closed=!started||CloseDMA();state.transferUs=micros()-stamp;
    if(!okay||!closed){if(DMA_State!=DMA_S_DisableReady)AGIDMAEmergencyRelease();state.valid=false;return false;}
    return true;
#else
    return false;
#endif
}
static FLASHMEM bool transferNuflixSlice(){
#if defined(FeatVMVideoDMA) && defined(Fab04_FullDMACapable)
    if(!videoBorderGrant)return true;
    const uint32_t grant=videoBorderCycles;videoBorderGrant=false;
    if((videoTiming&0xfc)!=0x88)return false;
    if(uint32_t(ARM_DWT_CYCCNT-grant)>F_CPU_ACTUAL/1000000u*500u)return true;
    auto &video=indexedVideo;auto &state=*static_cast<NuflixState *>(video.nuflix);
    nS_DMASetup=(videoTiming&1)?Def_nS_DMASetupNTSC:Def_nS_DMASetupPAL;
    nS_MaxAdj=(videoTiming&1)?Def_nS_MaxAdjNTSC:Def_nS_MaxAdjPAL;
    uint8_t bytes[256];bool started=false,okay=true;const uint32_t stamp=micros();
    while(video.streamOffset<848&&okay){
        const unsigned first=video.streamOffset;
        if(!(state.dirty[first/8]&(1u<<(first&7)))){++video.streamOffset;continue;}
        if(uint32_t(ARM_DWT_CYCCNT-grant)>F_CPU_ACTUAL/1000000u*((videoTiming&1)?2300u:4300u))break;
        unsigned last=first;
        do{if(nuflixCriticalChunk(last)){okay=false;break;}++last;}while(last<848&&last-first<8&&(state.dirty[last/8]&(1u<<(last&7))));
        if(!okay)break;
        const unsigned address=0x1000+first*32,length=(last-first)*32;
        for(unsigned offset=0;offset<length;++offset)bytes[offset]=dosvm_nuflix::displayByte(state.display,address+offset);
        okay=started?AGIContinueDMA(false,address,bytes,length,false):PerformDMA(false,address,bytes,length,false);
        if(okay){started=true;video.streamOffset=uint16_t(last);state.uploadedBytes+=length;}
    }
    const bool closed=!started||CloseDMA();state.transferUs+=micros()-stamp;
    if(!okay||!closed){if(DMA_State!=DMA_S_DisableReady)AGIDMAEmergencyRelease();state.valid=false;return false;}
    if(video.streamOffset==848){videoBorderWaiting=videoBorderGrant=false;video.phase=7;}
    return true;
#else
    return false;
#endif
}
#endif
