// Full-height F5 transport: source dirtiness and each destination's exact
// dirtiness are independent. A stale hidden bank must never become visible.
#include "helpers/indexed_video_fixture.h"
#include <fstream>
#include <string>
struct Reader { uint8_t *pixels; unsigned calls; };
static uint8_t pixel(void *p,uint16_t x,uint16_t y){
    auto &r=*static_cast<Reader *>(p);++r.calls;return r.pixels[y*320+x];
}
static void finish(VmIndexedDirtyRasterFrame &source){
    VmPacket packet{};assert(indexedVideoPacket(packet));indexedVideoAck();
    if(indexedVideo.phase==2){assert(transferIndexedVideo());indexedVideo.phase=3;}
    else {
        uint8_t visible[16384];const auto address=indexedVideo.activeBank?0x8000:0x4000;
        memcpy(visible,c64+address,sizeof visible);unsigned grants=0;
        while(indexedVideo.phase==6){
            const auto before=indexedVideo.uploadedBytes;indexedVideoBorder();assert(transferIndexedVideoSlice());
            assert(indexedVideo.uploadedBytes-before<=unsigned(videoTiming&1?1600:3200));
            assert(!memcmp(visible,c64+address,sizeof visible));assert(++grants<24);
        }
    }
    indexedVideoAck();assert(submitIndexedVideo(&source.raster.frame)==VmVideoResult::Transferred);
}
static void checkPixels(const uint8_t *pixels){
    const auto bank=indexedVideo.activeBank;
    for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++){
        const unsigned cell=y/8*40+x/8,plane=(y&7)/2;
        const auto bits=c64[(bank?0xa000:0x6000)+cell*8+y%8];
        const auto attr=c64[(bank?0x8c00:0x5c00)-plane*0x400+cell];
        const unsigned sourceX=x<24?0:((x-24)*320+160)/296;
        assert((bits&(0x80>>(x&7))?attr>>4:attr&15)==(x<24?0:pixels[y*320+sourceX]));
    }
    // The test host stages sprites but does not execute the timed kernel.
    // Five expanded black sprites cover only the unused physical margin;
    // their pattern and every changing screen map's pointers follow the bank.
    assert(c64[0xd015]==0&&c64[0xd017]==31);
    for(unsigned sprite=0;sprite<5;sprite++){
        assert(c64[0xd000+sprite*2]==24&&c64[0xd001+sprite*2]==52+sprite*42);
        assert(c64[0xd027+sprite]==0);
        for(unsigned plane=0;plane<4;plane++)
            assert(c64[(bank?0x8ff8:0x5ff8)-plane*0x400+sprite]==0xfd);
    }
    for(unsigned i=0;i<64;i++)assert(c64[(bank?0xbf40:0x7f40)+i]==(i<63?0xff:0));
}
int main(int argc,char **argv){
    auto arena=VirtualAlloc((void *)0x20010000,0x40000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(arena==(void *)0x20010000);auto p=(uint8_t *)VM_DATA_BASE;
    auto pixels=p+VM_FULL_VIDEO_WORKSPACE_BYTES+32,palette=pixels+64000,dirty=palette+48;
    auto reader=reinterpret_cast<Reader *>(dirty+128);*reader={pixels,0};
    const uint8_t rgb[8][3]={{0,0,0},{255,255,255},{136,57,50},{103,182,189},{139,63,150},{85,160,73},{64,49,141},{191,206,114}};
    memcpy(palette,rgb,sizeof rgb);
    VmCenterVideoSetup setup{{sizeof(setup),p,VM_FULL_VIDEO_WORKSPACE_BYTES,2,4,VM_INDEXED_FULL_F5|VM_INDEXED_SEPARATE_SELECTORS},0,25,0};
    setup.setup.workspace_bytes--;assert(!configureIndexedVideo(&setup.setup));++setup.setup.workspace_bytes;
    setup.first_row=1;assert(!configureIndexedVideo(&setup.setup));setup.first_row=0;
    setup.row_count=24;assert(!configureIndexedVideo(&setup.setup));setup.row_count=25;
    memset(p+VM_FULL_VIDEO_WORKSPACE_BYTES,0xa5,32);
    VmIndexedDirtyRasterFrame source{};auto &r=source.raster;auto &f=r.frame;
    f={sizeof(source),0,nullptr,palette,0,sizeof rgb,320,200,0,8,0};
    r.read_pixel=pixel;r.context=reader;source.source_dirty=dirty;
    for(uint8_t timing:{0x82,0x83}){
        memset(c64,0,sizeof c64);assert(configureIndexedVideo(&setup.setup));videoTiming=timing;
        for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++)pixels[y*320+x]=(y>=196?4:((y&7)/2)*2)+(x&1);
        source.source_dirty=(const uint8_t *)(VM_DATA_LIMIT-124);r.source_consumed=0;
        assert(submitIndexedVideo(&f)==VmVideoResult::Failed&&!r.source_consumed);source.source_dirty=dirty;
        memset(dirty,0,125);reader->calls=0;++f.generation;r.source_consumed=0;
        // A first image must ignore an empty dirty hint.
        assert(submitIndexedVideo(&f)==VmVideoResult::Busy);assert(r.source_consumed==1&&reader->calls==59200);
        assert(!configureIndexedVideo(nullptr));finish(source);checkPixels(pixels);
        assert(!indexedVideo.bankValid[1]);
        const auto beforeSegments=segments;++f.generation;r.source_consumed=0;reader->calls=0;
        assert(submitIndexedVideo(&f)==VmVideoResult::Transferred);
        assert(reader->calls==0&&segments==beforeSegments&&!indexedVideo.phase&&r.source_consumed==1);
        assert(!indexedVideo.bankValid[1]); // do not initialize a hidden bank just to repeat a picture
        // A change followed by a reversion while the first change uploads must
        // leave both destinations correct, and preserve immutable upload bytes.
        constexpr unsigned cell=411;auto edit=[&](bool change){
            for(unsigned y=80;y<88;y++)for(unsigned x=88;x<96;x++)pixels[y*320+x]=((y&7)/2)*2+((x&1)^(change?1:0));
            dirty[cell/8]|=1u<<(cell&7);
        };
        edit(true);++f.generation;r.source_consumed=0;reader->calls=0;
        assert(submitIndexedVideo(&f)==VmVideoResult::Busy);assert(reader->calls==128);
        auto frozen=*indexedVideo.full;memset(dirty,0,125);edit(false);
        assert(submitIndexedVideo(&f)==VmVideoResult::Busy);
        assert(!memcmp(&frozen,indexedVideo.full,sizeof frozen));finish(source);
        assert(dirty[cell/8]&(1u<<(cell&7))); // caller's next-generation writes were not cleared by transport
        ++f.generation;r.source_consumed=0;reader->calls=0;
        assert(submitIndexedVideo(&f)==VmVideoResult::Busy);assert(reader->calls==128);finish(source);checkPixels(pixels);
        assert(indexedVideo.uploadedBytes<256);
        // Matching the visible image can leave the other bank stale. A later
        // unrelated change must also repair that old change before the flip.
        memset(dirty,0,125);++f.generation;r.source_consumed=0;reader->calls=0;
        assert(submitIndexedVideo(&f)==VmVideoResult::Transferred);assert(reader->calls==0);
        pixels[0]=1;dirty[0]=1;++f.generation;r.source_consumed=0;
        assert(submitIndexedVideo(&f)==VmVideoResult::Busy);finish(source);checkPixels(pixels);
        assert(!indexedVideo.kernelNeeded&&indexedVideo.uploadedBytes<512);
        for(unsigned i=0;i<32;i++)assert(p[VM_FULL_VIDEO_WORKSPACE_BYTES+i]==0xa5);
        if(argc>1)for(unsigned bank=0;bank<2;bank++){
            auto &v=indexedVideo;const auto n=mpe_video::buildKernel(*v.frame,timing&1,v.kernel,4096,bank?0xc000:0x3000,true,false,true);
            assert(n&&n<=4096);
            std::ofstream file(std::string(argv[1])+"/kernel-"+(bank?"bank1-":"")+"full8x2-"+(timing&1?"ntsc":"pal")+".bin",std::ios::binary);
            file.write((char *)v.kernel,n);assert(file);
        }
        // Native replacement invalidates both output banks and conversion,
        // even if the producer's source dirty hint is empty on re-entry.
        memset(dirty,0,125);indexedVideoLegacy();++f.generation;r.source_consumed=0;reader->calls=0;
        assert(submitIndexedVideo(&f)==VmVideoResult::Busy);assert(reader->calls==59200);
        finish(source);checkPixels(pixels);
        assert(configureIndexedVideo(nullptr));
    }
    printf("PASS: full 8x2 fitted to 296x200, both banks/standards, black margin, unchanged zero conversion/handshake, mapped dirty conversion, pending reversion and stale-bank repair; workspace=%u actual=%zu\n",VM_FULL_VIDEO_WORKSPACE_BYTES,sizeof(mpe_video::FullFrame)+sizeof(mpe_video::LiveConverter)+mpe_video::KernelCapacity);
    VirtualFree(arena,0,MEM_RELEASE);
}
