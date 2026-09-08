// Four-map 8x2 keeps the complete fitted picture and masks only unused margin.
#include "vm/video/mpe_video_kernel.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
int main(int argc,char **argv){
    mpe_video::LiveFrame frame{};frame.mask=0x1ffffff;
    memset(frame.split,2,sizeof frame.split);
    uint8_t kernel[mpe_video::KernelCapacity+16];memset(kernel,0xa5,sizeof kernel);
    for(bool ntsc:{false,true})for(unsigned base:{0x3000,0xc000}){
        assert(!mpe_video::buildKernel(frame,ntsc,kernel,mpe_video::KernelCapacity,base,true));
        const auto n=mpe_video::buildKernel(frame,ntsc,kernel,mpe_video::KernelCapacity,base,true,false,true);
        assert(n&&n<=mpe_video::KernelCapacity);
        unsigned writes=0,enables=0,expansion=0;
        for(unsigned i=0;i+2<n;i++)if(kernel[i]==0x8d){
            const unsigned address=kernel[i+1]|unsigned(kernel[i+2])<<8;
            if(address==0xd011)++writes;
            if(address==0xd015)++enables;
            if(address==0xd017)++expansion;
        }
        assert(writes==200&&enables==2&&expansion==2);
        for(unsigned i=0;i<mpe_video::FullMaskBytes;i++){
            const auto address=mpe_video::fullMaskAddress(base==0xc000,i);
            assert(address==(i<64?(base==0xc000?0xbf40:0x7f40)+i:
                (base==0xc000?0x8ff8:0x5ff8)-(i-64)/5*0x400+(i-64)%5));
            assert(mpe_video::fullMaskByte(i)==(i<63?0xff:i==63?0:0xfd));
        }
        for(unsigned i=mpe_video::KernelCapacity;i<sizeof kernel;i++)assert(kernel[i]==0xa5);
        if(argc==2){
            const auto name=std::string(argv[1])+"/kernel-"+(base==0xc000?"bank1-":"")+"full8x2-"+(ntsc?"ntsc":"pal")+".bin";
            std::ofstream file(name,std::ios::binary);file.write((char *)kernel,n);assert(file);
        }
        printf("PASS full8x2 %s bank%u: %u bytes, 200 D011 writes, fitted-margin sprite enable/expansion/disable\n",ntsc?"NTSC":"PAL",base==0xc000,n);
    }
}
