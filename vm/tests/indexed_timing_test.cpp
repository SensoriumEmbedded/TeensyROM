// SPDX-License-Identifier: MIT
// Negotiated NUFLIX clients must also be able to upload ordinary F1/F3/F7.
#include "helpers/indexed_video_fixture.h"
int main(){
    assert(VirtualAlloc((void *)0x20010000,0x40000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)==(void *)0x20010000);
    auto workspace=reinterpret_cast<uint8_t *>(VM_DATA_BASE);
    auto pixels=workspace+VM_INDEXED_VIDEO_WORKSPACE_BYTES,palette=pixels+64000;
    memset(palette,0,6);palette[3]=palette[4]=palette[5]=255;
    for(unsigned y=0;y<200;++y)for(unsigned x=0;x<320;++x)pixels[y*320+x]=(x/8+y/8)&1;
    for(uint8_t mode:{0,1,3}){
        videoTiming=0x80;
        VmIndexedVideoSetup setup{sizeof setup,workspace,VM_INDEXED_VIDEO_WORKSPACE_BYTES,mode,15,VM_INDEXED_SEPARATE_SELECTORS};
        assert(configureIndexedVideo(&setup));
        VmIndexedFrame source{sizeof source,1,pixels,palette,64000,6,320,200,320,2,0};
        assert(submitIndexedVideo(&source)==VmVideoResult::Busy);
        assert(indexedVideo.frame->mode==mode);
        for(unsigned timing=0;timing<256;++timing){
            videoTiming=uint8_t(timing);segments=0;DMA_State=DMA_S_DisableReady;
            const bool valid=(timing>=0x80&&timing<=0x83)||timing==0x8e||timing==0x8f;
            assert(transferIndexedVideo()==valid);
            assert((segments!=0)==valid);
            assert(DMA_State==DMA_S_DisableReady&&videoTiming==timing);
            if(valid){assert(nS_DMASetup==((timing&1)?Def_nS_DMASetupNTSC:Def_nS_DMASetupPAL));
                assert(nS_MaxAdj==((timing&1)?Def_nS_MaxAdjNTSC:Def_nS_MaxAdjPAL));}
        }
    }
    puts("PASS indexed timing: F1/F3/F7 accept 80-83 and 8e/8f, preserve PAL/NTSC setup, reject every other byte before DMA");
}
