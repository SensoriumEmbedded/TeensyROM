#include "helpers/indexed_video_fixture.h"
int main(){
 auto arena=VirtualAlloc((void *)0x20010000,0x40000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(arena==(void *)0x20010000);
 auto p=(uint8_t *)VM_DATA_BASE;VmIndexedVideoSetup setup{sizeof setup,p,mpe_video::DeltaWorkspaceBytes,0,15,VM_INDEXED_SEPARATE_SELECTORS|VM_INDEXED_COLOR_F1|VM_INDEXED_SOLID_STATUS};
 assert(configureIndexedVideo(&setup));auto pixels=p+setup.workspace_bytes,palette=pixels+64000;
 for(unsigned i=0;i<256;i++)palette[i*3]=palette[i*3+1]=palette[i*3+2]=i;
 for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++)pixels[y*320+x]=x*256/320;
 VmIndexedFrame source{sizeof source,1,pixels,palette,64000,768,320,200,320,256,0};
 assert(submitIndexedVideo(&source)==VmVideoResult::Busy);assert(indexedVideo.frame->mode==0&&!indexedVideo.frame->mask);
 indexedVideoAck();assert(transferIndexedVideo());indexedVideo.phase=3;indexedVideoAck();
 assert(submitIndexedVideo(&source)==VmVideoResult::Transferred);
 const auto before=segments;source.generation++;assert(submitIndexedVideo(&source)==VmVideoResult::Transferred);assert(segments-before==76);
 assert(c64[0xd021]==indexedVideo.frame->background);
 for(unsigned c=0;c<1000;c++){assert(!memcmp(c64+0x6000+c*8,indexedVideo.frame->cells[c],8));assert(c64[0xd800+c]<16);}
 const auto color=*indexedVideo.frame;
 setup.reserved=VM_INDEXED_SEPARATE_SELECTORS;assert(configureIndexedVideo(&setup));source.generation++;
 assert(submitIndexedVideo(&source)==VmVideoResult::Busy);assert(memcmp(color.cells,indexedVideo.frame->cells,sizeof color.cells));
 // Both loan sizes retain their exact bounds and unsupported status-only
 // setup is rejected. Cache storage may not overlap either C64 bank image.
 for(unsigned bytes:{unsigned(VM_INDEXED_VIDEO_WORKSPACE_BYTES),mpe_video::DeltaWorkspaceBytes}){
  setup.workspace_bytes=bytes;setup.reserved=VM_INDEXED_SEPARATE_SELECTORS|VM_INDEXED_COLOR_F1|VM_INDEXED_SOLID_STATUS;
  assert(configureIndexedVideo(&setup));
  assert(indexedVideo.colorCache&&(uint8_t *)(indexedVideo.colorCache+1)<=p+bytes);
  if(indexedVideo.bank[1])assert((uint8_t *)indexedVideo.colorCache>=(uint8_t *)(indexedVideo.bank[1]+1));
 }
 setup.reserved=VM_INDEXED_SOLID_STATUS;assert(!configureIndexedVideo(&setup));
 printf("PASS Color F1 negotiated through firmware; ordinary 76-segment transfer; legacy opt-out; bounded cache; frame %zu, converter %zu bytes\n",sizeof(mpe_video::LiveFrame),sizeof(mpe_video::LiveConverter));
}
