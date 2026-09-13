// Immutable video sources may use guest RAM2; writable workspace stays RAM1.
#include "helpers/indexed_video_fixture.h"
static uint8_t readPixel(void *p,uint16_t x,uint16_t y){return static_cast<uint8_t *>(p)[y*320+x];}
int main(){
 assert(VirtualAlloc((void *)0x20010000,0x40000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)==(void *)0x20010000);
 assert(VirtualAlloc((void *)VM_RAM_BASE,VM_RAM2_GUEST_BYTES,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)==(void *)VM_RAM_BASE);
 auto ram1=(uint8_t *)VM_DATA_BASE,ram2=(uint8_t *)VM_RAM_BASE,end=ram2+VM_RAM2_GUEST_BYTES;
 assert(VM_RAM2_GUEST_BYTES==416*1024);
 for(unsigned bytes:{1u,125u,768u,64000u}){
  assert(videoSourceRange(end-bytes,bytes));assert(!videoSourceRange(end-bytes+1,bytes));
 }
 assert(!videoSourceRange(end,1)&&!videoRange(ram2,1));
 VmIndexedVideoSetup setup{sizeof setup,ram2,VM_INDEXED_VIDEO_WORKSPACE_BYTES,0,15,0};
 assert(!configureIndexedVideo(&setup));setup.workspace=ram1;assert(configureIndexedVideo(&setup));
 const auto palette=ram2+64000;memset(ram2,0,64000);memset(palette,0,768);palette[3]=palette[4]=palette[5]=255;
 for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++)ram2[y*320+x]=(x+y)&1;
 VmIndexedFrame source{sizeof source,1,ram2,palette,64000,768,320,200,320,256,0};
 auto bad=source;bad.pixels=end-63999;assert(submitIndexedVideo(&bad)==VmVideoResult::Failed);
 bad=source;bad.palette=end-767;assert(submitIndexedVideo(&bad)==VmVideoResult::Failed);
 assert(submitIndexedVideo(&source)==VmVideoResult::Busy);
 indexedVideoAck();assert(transferIndexedVideo());indexedVideo.phase=3;indexedVideoAck();
 assert(submitIndexedVideo(&source)==VmVideoResult::Transferred);
 assert(configureIndexedVideo(nullptr));
 VmCenterVideoSetup full{{sizeof full,ram1,VM_FULL_VIDEO_WORKSPACE_BYTES,2,4,VM_INDEXED_FULL_F5|VM_INDEXED_SEPARATE_SELECTORS},0,25,0};
 assert(configureIndexedVideo(&full.setup));
 VmIndexedDirtyRasterFrame raster{};raster.raster.frame=source;
 raster.raster.frame.bytes=sizeof raster;raster.raster.frame.pixels=nullptr;raster.raster.frame.pixel_bytes=0;
 raster.raster.read_pixel=readPixel;raster.raster.context=ram2;raster.source_dirty=end-125;memset(end-125,255,125);
 auto invalid=raster;invalid.source_dirty=end-124;assert(submitIndexedVideo(&invalid.raster.frame)==VmVideoResult::Failed);
 invalid=raster;invalid.raster.context=end;assert(submitIndexedVideo(&invalid.raster.frame)==VmVideoResult::Failed);
 assert(submitIndexedVideo(&raster.raster.frame)==VmVideoResult::Busy);
 assert(raster.raster.source_consumed==1);
 puts("PASS RAM2 indexed pixels/palette/raster context/dirty map; exact 416 KiB boundary; RAM1-only workspace");
}
