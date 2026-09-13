struct NuflixState {
    dosvm_nuflix::NativeDisplay display;
    uint32_t hashes[2][53];
    uint8_t dirty[7],palette[48],map[16];
    uint16_t width,height,geometry,mirrorOperand,fitRegions;
    uint32_t conversionUs,transferUs,uploadedBytes;
    uint8_t unchangedBands,packedBands,colorCount;
    bool valid;
};
static_assert(sizeof(NuflixState)<=VM_NUFLIX_VIDEO_WORKSPACE_BYTES,"Double-buffer metadata exceeds existing DOS loan");
static FLASHMEM uint32_t nuflixBlockHash(const dosvm_nuflix::NativeDisplay &display,unsigned block){
    uint32_t hash=2166136261u;
    for(unsigned offset=0;offset<512;++offset)hash=(hash^dosvm_nuflix::displayByte(display,0x1000+block*512+offset))*16777619u;
    return hash;
}
static FLASHMEM bool configureNuflix(const VmIndexedVideoSetup *setup){
    if(indexedVideo.phase||setup->bytes!=sizeof(VmCenterVideoSetup)||setup->workspace_bytes<VM_NUFLIX_VIDEO_WORKSPACE_BYTES||
       ((uintptr_t)setup->workspace&3)||!videoRange(setup->workspace,VM_NUFLIX_VIDEO_WORKSPACE_BYTES)||setup->default_mode!=2||setup->capabilities!=4||
       (videoTiming&0xfe)!=0x8e)return false;
    const auto &center=*reinterpret_cast<const VmCenterVideoSetup *>(setup);
    if(center.first_row||center.row_count!=25||center.reserved)return false;
    indexedVideo={};videoBorderWaiting=videoBorderGrant=false;
    indexedVideo.nuflix=setup->workspace;memset(setup->workspace,0,sizeof(NuflixState));
    indexedVideo.geometry=setup->reserved;indexedVideo.requested=indexedVideo.preferred=2;indexedVideo.capabilities=4;indexedVideo.configured=true;return true;
}
struct NuflixSource {VmIndexedRasterFrame *raster;const uint8_t *map;};
static FLASHMEM uint8_t readNuflixPixel(const void *context,unsigned column,unsigned row){
    const auto &source=*static_cast<const NuflixSource *>(context);const auto &frame=source.raster->frame;
    return source.map[source.raster->read_pixel(source.raster->context,column*frame.width/320,row*frame.height/200)&15];
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
    const bool previous=state.valid&&video.displayReady&&video.bankValid[video.activeBank];
    const bool same=previous&&state.width==source->width&&state.height==source->height&&state.colorCount==source->colors&&state.geometry==raster.geometry&&!memcmp(state.palette,source->palette,48);
    uint8_t changed=0;if(dirty)for(unsigned index=0;index<125;++index)changed|=dirty[index];
    if(same&&dirty&&!changed){raster.source_consumed=1;source->resolved_mode=2;raster.resolved_background=0;return VmVideoResult::Transferred;}
    const uint32_t started=micros();state.valid=false;
    static constexpr uint8_t rgb[16][3]={{0,0,0},{255,255,255},{136,57,50},{103,182,189},{139,63,150},{85,160,73},{64,49,141},{191,206,114},{139,84,41},{87,66,0},{184,105,98},{80,80,80},{120,120,120},{148,224,137},{120,105,196},{159,159,159}};
    memset(state.map,0,sizeof(state.map));
    for(unsigned color=0;color<source->colors;++color){
        unsigned minimum=UINT32_MAX,selected=0;
        for(unsigned candidate=0;candidate<16;++candidate){
            unsigned distance=0;for(unsigned channel=0;channel<3;++channel){int delta=int(source->palette[color*3+channel])-rgb[candidate][channel];distance+=delta*delta;}
            if(distance<minimum){minimum=distance;selected=candidate;}
        }
        state.map[color]=selected;
    }
    NuflixSource pixels{&raster,state.map};dosvm_nuflix::NativeSource reader{&pixels,readNuflixPixel};dosvm_nuflix::NativeConverter colors;
    const auto cached=dosvm_nuflix::CachedConverter::convert(colors,reader,state.display,(videoTiming&1)!=0,dosvm_nuflix::displayTemplate+0x2140,dirty,same);
    if(!cached.success)return VmVideoResult::Failed;
    state.mirrorOperand=uint16_t(dosvm_nuflix::mirroredIndexedOperand(state.display));
    state.fitRegions=uint16_t(cached.fit.regions);state.unchangedBands=uint8_t(cached.unchangedBands);state.packedBands=uint8_t(cached.packedBands);
    video.targetBank=previous?1-video.activeBank:0;
    uint32_t nextHashes[53];
    memset(state.dirty,0,sizeof(state.dirty));bool any=!previous;
    for(unsigned block=0;block<53;++block){
        const uint32_t hash=nuflixBlockHash(state.display,block);
        nextHashes[block]=hash;
        if(previous&&hash!=state.hashes[video.activeBank][block])any=true;
        if(!video.bankValid[video.targetBank]||hash!=state.hashes[video.targetBank][block])state.dirty[block/8]|=1u<<(block&7);
    }
    state.width=source->width;state.height=source->height;state.geometry=raster.geometry;state.colorCount=uint8_t(source->colors);memcpy(state.palette,source->palette,48);
    state.conversionUs=micros()-started;state.valid=true;raster.source_consumed=1;raster.resolved_background=0;source->resolved_mode=2;
    if(!any)return VmVideoResult::Transferred;
    memcpy(state.hashes[video.targetBank],nextHashes,sizeof nextHashes);
    video.bankValid[video.targetBank]=false;video.generation=source->generation;video.phase=previous?5:1;video.streamOffset=0;
    videoBorderWaiting=previous;videoBorderGrant=false;state.transferUs=state.uploadedBytes=0;return VmVideoResult::Busy;
}
static FLASHMEM void nuflixTransferComplete(){
    indexedVideo.bankValid[indexedVideo.targetBank]=true;
}
static FLASHMEM bool nuflixTransfer(bool sliced){
#if defined(FeatVMVideoDMA) && defined(Fab04_FullDMACapable)
    auto &video=indexedVideo;auto &state=*static_cast<NuflixState *>(video.nuflix);
    if((videoTiming&0xfe)!=0x8e)return false;
    uint32_t grant=0;
    if(sliced){
        if(!videoBorderGrant)return true;
        grant=videoBorderCycles;videoBorderGrant=false;
        if(uint32_t(ARM_DWT_CYCCNT-grant)>F_CPU_ACTUAL/1000000u*250u)return true;
    }
    nS_DMASetup=(videoTiming&1)?Def_nS_DMASetupNTSC:Def_nS_DMASetupPAL;nS_MaxAdj=(videoTiming&1)?Def_nS_MaxAdjNTSC:Def_nS_MaxAdjPAL;
    uint8_t bytes[256];bool started=false,okay=true;const uint32_t stamp=micros();unsigned payload=0;
    if(sliced){
        bytes[0]=0;okay=PerformDMA(true,0x06a0,bytes,1,false);started=okay;
        if(okay&&bytes[0]!=0xa5){
            if(!CloseDMA()){if(DMA_State!=DMA_S_DisableReady)AGIDMAEmergencyRelease();state.valid=false;video.bankValid[video.targetBank]=false;return false;}
            if(uint32_t(ARM_DWT_CYCCNT-grant)<=F_CPU_ACTUAL/1000000u*250u)videoBorderGrant=true;
            return true;
        }
    }
    while(video.streamOffset<106&&okay){
        const unsigned half=video.streamOffset,block=half/2;
        if(!(state.dirty[block/8]&(1u<<(block&7)))){++video.streamOffset;continue;}
        if(sliced&&(payload>=((videoTiming&1)?1536u:3072u)||uint32_t(ARM_DWT_CYCCNT-grant)>F_CPU_ACTUAL/1000000u*((videoTiming&1)?1800u:3800u)))break;
        const unsigned address=0x1000+half*256;
        for(unsigned offset=0;offset<256;++offset)bytes[offset]=dosvm_nuflix::bankDisplayByte(state.display,address+offset,video.targetBank,state.mirrorOperand);
        okay=started?AGIContinueDMA(false,address+video.targetBank*0x8000,bytes,256,false):PerformDMA(false,address,bytes,256,false);
        if(okay){started=true;++video.streamOffset;state.uploadedBytes+=256;payload+=256;}
    }
    if(sliced&&okay&&started){bytes[0]=1;okay=AGIContinueDMA(false,0x06a1,bytes,1,false);}
    const bool closed=!started||CloseDMA();state.transferUs+=micros()-stamp;
    if(!okay||!closed){if(DMA_State!=DMA_S_DisableReady)AGIDMAEmergencyRelease();state.valid=false;video.bankValid[video.targetBank]=false;return false;}
    if(video.streamOffset==106){nuflixTransferComplete();if(sliced){videoBorderWaiting=videoBorderGrant=false;video.phase=7;}}
    return true;
#else
    return false;
#endif
}
static FLASHMEM bool transferNuflix(){return nuflixTransfer(false);}
static FLASHMEM bool transferNuflixSlice(){return nuflixTransfer(true);}
