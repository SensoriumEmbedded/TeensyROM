// Negotiated production F5: exact repairs, finite hardware coverage, geometry.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <fstream>
#include <filesystem>
#include "../video/mpe_video_live.cpp"
using namespace mpe_video;
static const uint8_t rgb[16][3]={{0,0,0},{255,255,255},{136,57,50},{103,182,189},
 {139,63,150},{85,160,73},{64,49,141},{191,206,114},{139,84,41},{87,66,0},
 {184,105,98},{80,80,80},{120,120,120},{148,224,137},{120,105,196},{159,159,159}};
static uint8_t pixels[640*240],tagged[sizeof pixels];
static uint8_t shown(const LiveFrame &f,unsigned x,unsigned y,bool overlay=true){
 const auto c=f.cells[y/8*40+x/8];const uint8_t result=(c[y%8]&(128>>(x%8)))?c[8]>>4:c[8]&15;
 if(!f.overlays||!overlay)return result;
 const auto byte=[&](unsigned n){return f.cells[n][9];};
 for(unsigned n=0;n<8;n++)if(byte(529)&(1<<n)){
  const int dx=int(x)+24-int(byte(512+n*2)+((byte(528)&(1<<n))?256:0)),dy=int(y)+50-int(byte(513+n*2));
  if(dx>=0&&dx<24&&dy>=0&&dy<21&&(byte(n*64+dy*3+dx/8)&(128>>(dx%8))))return byte(530+n);
 }
 return result;
}
static uint8_t read(void *p,uint16_t x,uint16_t y){auto &s=*static_cast<IndexedSource *>(p);return s.pixels[y*s.stride+x];}
static void save(const char *directory,const char *name,const LiveFrame &f){
 std::filesystem::create_directories(directory);const auto path=std::filesystem::path(directory)/name;
 uint8_t packed[10000],expected[64000];
 for(unsigned i=0;i<1000;i++){memcpy(packed+i*8,f.cells[i],8);packed[8000+i]=f.cells[i][8];packed[9000+i]=f.cells[i][9];}
 for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++)expected[y*320+x]=shown(f,x,y);
 std::ofstream b(path.string()+".bin",std::ios::binary);b.write((const char *)packed,sizeof packed);assert(b);
 std::ofstream e(path.string()+".expected",std::ios::binary);e.write((const char *)expected,sizeof expected);assert(e);
}
int main(int argc,char **argv){
 LiveConverter cv;LiveFrame f5{},f7{},plain{},hinted{};
 IndexedSource s{pixels,&rgb[0][0],320,200,320,16,128};
 // Three and four colors in the SAME eight-pixel row, including all edges
 // and hardware X-MSB positions. Each fixture fits within eight sprites.
 for(unsigned x0:{0u,144u,288u})for(unsigned y0:{0u,84u,189u})for(unsigned colors:{3u,4u}){
  memset(pixels,0,sizeof pixels);
  const uint8_t colorset[]={0,3,4,1};
  for(unsigned y=y0;y<y0+21&&y<200;y++)for(unsigned x=x0;x<x0+24&&x<320;x++)pixels[y*320+x]=colorset[(x+y)%colors];
  assert(cv.render(s,2,f5)&&cv.render(s,3,f7)&&f5.overlays&&!f5.mask);
  for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++)assert(shown(f5,x,y)==pixels[y*320+x]);
  for(unsigned c=0;c<1000;c++)assert(!memcmp(f5.cells[c],f7.cells[c],9));
 }
 // Crowded frame exceeds sprite coverage. Every changed pixel must still be
 // an exact correction, with bounded mask/pointers and deterministic output.
 uint32_t random=1;for(auto &p:pixels){random=random*1664525+1013904223;p=random>>28;}
 assert(cv.render(s,2,f5)&&cv.render(s,3,f7));unsigned repaired=0;
 for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++){
  const auto a=shown(f5,x,y),b=shown(f7,x,y);assert(a==b||a==pixels[y*320+x]);repaired+=a!=b;
 }
 assert(repaired&&f5.cells[529][9]==255);const auto saved=f5;
 assert(cv.render(s,2,f5,&f5)&&!memcmp(&saved,&f5,sizeof saved));
 auto raster=s;raster.context=&s;raster.pixels=nullptr;raster.read_pixel=read;
 assert(cv.render(raster,2,hinted)&&!memcmp(&f5,&hinted,sizeof f5));
 // Actor tags are metadata only: every non-F5 image remains byte-identical.
 s.width=s.stride=256;s.height=240;
 for(unsigned i=0;i<256*240;i++)tagged[i]=pixels[i]|64;
 auto taggedSource=s;taggedSource.pixels=tagged;taggedSource.geometry=128|256;
 for(uint8_t mode:{0,1,3}){s.geometry=0;assert(cv.render(s,mode,plain)&&cv.render(taggedSource,mode,hinted));assert(!memcmp(&plain,&hinted,sizeof plain));}
 assert(cv.render(taggedSource,2,f5)&&cv.render(taggedSource,3,f7));
 for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++)if(x<32||x>=288)assert(!shown(f5,x,y)&&!shown(f7,x,y));
 static_assert(sizeof(LiveFrame)==10036,"No image/workspace growth");
 if(argc==2){save(argv[1],"nes-f5",f5);save(argv[1],"nes-f7",f7);
  s.width=s.stride=320;s.height=200;s.geometry=128;memset(pixels,0,sizeof pixels);
  for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++){
   const uint8_t colors[]={0,3,4,1};pixels[y*320+x]=(x+y)&1?3:0;
   if((x<24&&y<21)||(x>=288&&y>=189))pixels[y*320+x]=colors[(x+y)%4];
  }
  assert(cv.render(s,2,f5)&&cv.render(s,3,f7));save(argv[1],"dos-f5",f5);save(argv[1],"dos-f7",f7);
 }
 puts("PASS: production F5 three/four same-row colors at all edges, exact repairs under saturation, reader parity, deterministic packing, NES F1/F3/F7 tag parity and centered F5/F7");
}
