// Exercise the real shared firmware converter/transport with a bounded loan.
#include "helpers/indexed_video_fixture.h"
#include <fstream>
#include <string>
int main(int argc,char **argv){
    auto arena=VirtualAlloc((void *)0x20010000,0x40000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(arena==(void *)0x20010000);auto p=(uint8_t *)VM_DATA_BASE;
    VmCenterVideoSetup setup{{sizeof(setup),p,VM_CENTER_VIDEO_WORKSPACE_BYTES,2,4,VM_INDEXED_CENTER_F5|VM_INDEXED_SEPARATE_SELECTORS},8,9,0};
    setup.setup.workspace_bytes--;assert(!configureIndexedVideo(&setup.setup));setup.setup.workspace_bytes++;
    setup.row_count=10;assert(!configureIndexedVideo(&setup.setup));setup.row_count=9;
    setup.first_row=17;assert(!configureIndexedVideo(&setup.setup));setup.first_row=8;
    memset(p+VM_CENTER_VIDEO_WORKSPACE_BYTES,0xa5,32);
    auto pixels=p+VM_CENTER_VIDEO_WORKSPACE_BYTES+32,palette=pixels+64000;
    const uint8_t rgb[8][3]={{0,0,0},{255,255,255},{136,57,50},{103,182,189},{139,63,150},{85,160,73},{64,49,141},{191,206,114}};
    memcpy(palette,rgb,sizeof rgb);
    VmIndexedFrame source{sizeof(source),0,pixels,palette,64000,sizeof rgb,320,200,320,8,0};
    for(uint8_t timing:{0x82,0x83}){
        assert(configureIndexedVideo(&setup.setup));videoTiming=timing;
        for(unsigned f=0;f<8;f++){
            for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++)
                pixels[y*320+x]=(y>=64&&y<136?((y&7)/2)*2:0)+(x&1);
            // Reversion is also dirty for both banks, even if it matches an
            // older displayed frame. The target must always be byte-exact.
            if(f==4)for(unsigned y=80;y<88;y++)for(unsigned x=104;x<112;x++)pixels[y*320+x]=7;
            ++source.generation;assert(submitIndexedVideo(&source)==VmVideoResult::Busy);
            assert(!configureIndexedVideo(nullptr));assert(!configureIndexedVideo(&setup.setup));
            const unsigned target=indexedVideo.targetBank;VmPacket packet{};
            assert(indexedVideoPacket(packet));indexedVideoAck();
            if(indexedVideo.phase==2){assert(transferIndexedVideo());indexedVideo.phase=3;}
            else{
                unsigned grants=0;uint8_t visible[16384];memcpy(visible,c64+(target?0x4000:0x8000),sizeof visible);
                while(indexedVideo.phase==6){
                    const auto before=indexedVideo.uploadedBytes;indexedVideoBorder();assert(transferIndexedVideoSlice());
                    assert(indexedVideo.uploadedBytes-before<=unsigned(timing&1?1600:3200));
                    assert(!memcmp(visible,c64+(target?0x4000:0x8000),sizeof visible));assert(++grants<16);
                }
                if(f==2||f==3||f==7)assert(indexedVideo.uploadedBytes==0&&!indexedVideo.kernelNeeded);
                if(f==4||f==5||f==6)assert(indexedVideo.uploadedBytes>0&&indexedVideo.uploadedBytes<256);
            }
            // Decode every destination pixel, all four color maps, both banks.
            for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++){
                const unsigned cell=y/8*40+x/8,plane=y>=64&&y<136?(y&7)/2:0;
                const auto bits=c64[(target?0xa000:0x6000)+cell*8+y%8];
                const auto attr=c64[(target?0x8c00:0x5c00)-plane*0x400+cell];
                assert((bits&(0x80>>(x&7))?attr>>4:attr&15)==pixels[y*320+x]);
            }
            indexedVideoAck();assert(submitIndexedVideo(&source)==VmVideoResult::Transferred);
            for(unsigned i=0;i<mpe_video::CenterMaskBytes;i++)
                assert(c64[mpe_video::centerMaskAddress(target,i)]==mpe_video::centerMaskByte(i,setup.row_count));
            for(unsigned i=0;i<32;i++)assert(p[VM_CENTER_VIDEO_WORKSPACE_BYTES+i]==0xa5);
        }
        if(argc>1)for(unsigned bank=0;bank<2;bank++){
            auto &v=indexedVideo;const auto n=mpe_video::buildKernel(*v.frame,timing&1,v.kernel,mpe_video::KernelCapacity,bank?0xc000:0x3000,true);
            assert(n&&n<=4096);
            std::ofstream file(std::string(argv[1])+"/kernel-"+(bank?"bank1-":"")+"center-"+(timing&1?"ntsc":"pal")+".bin",std::ios::binary);
            file.write((char *)v.kernel,n);assert(file);
        }
        assert(configureIndexedVideo(nullptr));assert(!indexedVideo.configured&&!indexedVideo.center);
    }
    // Every advertised band geometry leaves sprite DMA inside the kernel.
    for(unsigned first=1;first<=14;first++)for(unsigned rows=1;rows<=9;rows++){
        setup.first_row=first;setup.row_count=rows;assert(configureIndexedVideo(&setup.setup));
        mpe_video::IndexedSource src{pixels,palette,320,200,320,8};auto &v=indexedVideo;
        assert(v.converter->renderCenter(src,*v.center,first,rows));
        for(bool ntsc:{false,true})assert(mpe_video::buildKernel(*v.frame,ntsc,v.kernel,4096,0x3000,true));
        for(unsigned sprite=0;sprite<2;sprite++)for(unsigned row=0;row<21;row++)for(unsigned x=0;x<3;x++)
            assert(mpe_video::centerMaskByte(sprite*64+row*3+x,rows)==(sprite*42+row*2<rows*8?255:0));
    }
    assert(configureIndexedVideo(nullptr));
    printf("PASS: 16 KiB center F5, all pixels/four maps/two banks, unchanged frames, small changes/reversions, immutable visible bank, pending-loan rejection, release, PAL/NTSC budgets\n");
    VirtualFree(arena,0,MEM_RELEASE);return 0;
}
