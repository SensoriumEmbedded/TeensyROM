#include <cassert>
#include <cstdio>
#include <chrono>
#include <fstream>
#include "../video/mpe_video_live.cpp"
#include "../video/mpe_video_kernel.h"
using namespace mpe_video;
static uint8_t displayedPixel(const LiveFrame &frame,unsigned x,unsigned y){
    const auto cell=frame.cells[(y/8)*40+x/8];
    if(frame.mode==0){
        const unsigned index=(cell[y%8]>>(6-2*((x%8)/2)))&3;
        return index==0?0:index==1?cell[8]>>4:index==2?cell[8]&15:cell[9];
    }
    return cell[y%8]&(0x80>>(x%8))?cell[8]>>4:cell[8]&15;
}
static void checkNativeCentering(){
    static const uint8_t palette[2][3]={{0,0,0},{255,255,255}};
    static uint8_t pixels[391*240];
    LiveConverter converter;LiveFrame frame{};
    // NES geometry, odd/minimal widths, and unchanged full/wider sources.
    for(unsigned width:{256u,255u,1u,320u,384u}){
        const unsigned stride=width+7;
        memset(pixels,0xff,sizeof pixels);
        for(unsigned y=0;y<240;y++)for(unsigned x=0;x<width;x++)
            pixels[y*stride+x]=(x==0||x==width-1)?1:(x+y)&1;
        IndexedSource source{pixels,&palette[0][0],uint16_t(width),240,uint16_t(stride),2};
        // Switching away from Sharp must repaint both margins at full width.
        for(uint8_t mode:{3,0,1,2,3}){
            assert(converter.render(source,mode,frame,&frame));assert(!frame.mask);
            const bool centered=(mode==2||mode==3)&&width<320;
            const unsigned left=centered?(320-width)/2:0;
            for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++){
                uint8_t expected=0;
                if(!centered||(x>=left&&x<left+width)){
                    const unsigned dx=mode==0?(x|1):x;
                    const unsigned sx=centered?x-left:((2*dx+1)*width)/640;
                    const unsigned sy=((2*y+1)*240)/400;
                    expected=pixels[sy*stride+sx];
                }
                assert(displayedPixel(frame,x,y)==expected);
            }
        }
    }
    // Source palette index zero need not be black: side padding still must be.
    memset(pixels,0,sizeof pixels);
    IndexedSource white{pixels,&palette[1][0],256,240,256,1};
    assert(converter.render(white,3,frame));
    for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++)
        assert(displayedPixel(frame,x,y)==uint8_t(x>=32&&x<288));
}
int main(int argc,char **argv){
    checkNativeCentering();
    static const uint8_t palette[4][3]={{0,0,0},{255,255,255},{136,57,50},{103,182,189}};
    static uint8_t pixels[320*200];
    for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++)pixels[y*320+x]=((y&7)>=4?2:0)+(x&1);
    IndexedSource src{pixels,&palette[0][0],320,200,320,4};LiveConverter c;LiveFrame out{};
    assert(c.render(src,0,out)&&out.mode==0&&!out.mask);
    for(const auto &cell:out.cells)assert(cell[9]<16);
    assert(c.render(src,3,out)&&!out.mask);
    for(const auto &cell:out.cells)assert(cell[8]==cell[9]);
    assert(c.render(src,1,out)&&__builtin_popcount(out.mask)==8);
    for(unsigned i=0;i<25;i++)if(out.mask&(1u<<i))assert(out.split[i]==4);
    const auto prior=out;assert(c.render(src,1,out,&out));assert(!memcmp(&prior,&out,sizeof out));
    assert(c.render(src,2,out,&out)&&out.mask==0x1ffffff);
    for(auto split:out.split)assert(split==4);
    // Stable F5 is opt-in. It leaves all other modes byte-for-byte unchanged
    // and fixes the raster program even when the scene changes completely.
    for(uint8_t mode:{0,1,3}){
        LiveFrame legacy{},hinted{};src.geometry=0;assert(c.render(src,mode,legacy));
        src.geometry=64;assert(c.render(src,mode,hinted));assert(!memcmp(&legacy,&hinted,sizeof legacy));
    }
    src.geometry=64;assert(c.render(src,2,out));assert(out.mask==0x1ffffff);
    for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++){
        const auto cell=out.cells[y/8*40+x/8];const auto colors=cell[(y&7)<4?8:9];
        const auto got=(cell[y&7]&(128>>(x&7)))?colors>>4:colors&15;
        assert(got==pixels[y*320+x]);
    }
    const auto stablePlan=out;memset(pixels,0,sizeof pixels);assert(c.render(src,2,out,&out));
    assert(out.mask==stablePlan.mask&&!memcmp(out.split,stablePlan.split,25));
    for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++)pixels[y*320+x]=((y&7)>=4?2:0)+(x&1);
    src.geometry=0;assert(c.render(src,2,out));
    uint8_t kernel[KernelCapacity+16];memset(kernel,0xcc,sizeof kernel);
    for(bool ntsc:{false,true})for(unsigned base:{0x3000,0xc000}){
        unsigned n=buildKernel(out,ntsc,kernel,KernelCapacity,base);assert(n&&n<=KernelCapacity);
        for(unsigned i=KernelCapacity;i<sizeof kernel;i++)assert(kernel[i]==0xcc);
        if(argc==2){std::ofstream f(std::string(argv[1])+(base==0xc000?"-bank1":"")+(ntsc?"-ntsc.bin":"-pal.bin"),std::ios::binary);f.write((char *)kernel,n);}
    }
    if(argc==2)for(bool ntsc:{false,true})for(unsigned base:{0x3000,0xc000}){
        for(unsigned band=0;band<25;band++)out.split[band]=1+band%7;
        out.mask=0x1555555;unsigned n=buildKernel(out,ntsc,kernel,KernelCapacity,base);assert(n);
        std::ofstream f(std::string(argv[1])+(base==0xc000?"-bank1":"")+(ntsc?"-mixed-ntsc.bin":"-mixed-pal.bin"),std::ios::binary);f.write((char *)kernel,n);
    }
    for(bool ntsc:{false,true})for(unsigned split=1;split<=7;split++){
        out.mask=0x1ffffff;memset(out.split,split,sizeof out.split);
        assert(buildKernel(out,ntsc,kernel,KernelCapacity)<=KernelCapacity);
        for(unsigned i=KernelCapacity;i<sizeof kernel;i++)assert(kernel[i]==0xcc);
    }
    assert(!buildKernel(out,false,kernel,2));out.split[0]=0;assert(!buildKernel(out,false,kernel,KernelCapacity));
    src.stride=319;assert(!c.render(src,0,out));src.stride=320;assert(!c.render(src,4,out));
    auto start=std::chrono::steady_clock::now();for(unsigned i=0;i<100;i++)assert(c.render(src,1,out,&out));
    printf("PASS: centered native-width F5/F7, full-width F1/F3, live color/sharp, Auto-8 cap, all 25 useful bands, stable plans, bounded dual-bank PAL/NTSC kernels; host conversion %.2f ms/frame (not Teensy timing)\n",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/100);
}
