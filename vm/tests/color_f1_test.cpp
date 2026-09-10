// SPDX-License-Identifier: MIT
#include "../video/mpe_video_live.cpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <fstream>
#include <string>
static uint8_t pixels[64000],pal[768];
static unsigned shown(const mpe_video::LiveFrame &f,unsigned x,unsigned y){
 const auto cell=f.cells[y/8*40+x/8];const unsigned colors[]={f.background,unsigned(cell[8]>>4),unsigned(cell[8]&15),cell[9]};
 return colors[(cell[y%8]>>(6-(x%8)/2*2))&3];
}
int main(int argc,char **argv){
 using namespace mpe_video;
 LiveConverter cv,fresh;LiveFrame out{},other{};
 struct {uint32_t first=0xabcd1234;ColorF1Cache cache{};uint32_t last=0x4321dcba;} guarded;
 IndexedSource s{pixels,pal,320,200,320,256};s.color_f1=true;
 auto verify=[&](){
  assert(guarded.first==0xabcd1234&&guarded.last==0x4321dcba);
  assert(!out.mask&&!out.mode&&!out.background&&!out.overlays&&!out.multicolor);
  for(const auto &c:out.cells)assert(c[9]<16);
 };
 assert(cv.render(s,0,out,nullptr,&guarded.cache));verify();
 for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++)assert(shown(out,x,y)==0);
 memset(pal,255,sizeof pal);assert(cv.render(s,0,out,nullptr,&guarded.cache));verify();
 for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++)assert(shown(out,x,y)==1);
 uint32_t rng=19;auto random=[&](){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return uint8_t(rng);};
 for(unsigned test=0;test<24;test++){
  for(auto &c:pixels)c=random();for(auto &c:pal)c=random();
  s.solid_from_y=test&1?168:200;
  assert(cv.render(s,0,out,nullptr,&guarded.cache));assert(fresh.render(s,0,other));assert(!memcmp(&out,&other,sizeof out));verify();
  assert(cv.render(s,3,other));assert(cv.render(s,0,other,nullptr,&guarded.cache));assert(!memcmp(&out,&other,sizeof out));
  pal[test]^=127;assert(cv.render(s,0,out,&out,&guarded.cache));assert(fresh.render(s,0,other));assert(!memcmp(&out,&other,sizeof out));
 }
 // Opt-in cannot change other modes or non-native source geometries.
 for(unsigned mode=1;mode<4;mode++){
  s.color_f1=false;assert(cv.render(s,mode,out));s.color_f1=true;assert(fresh.render(s,mode,other));assert(!memcmp(&out,&other,sizeof out));
 }
 s.width=256;s.color_f1=false;assert(cv.render(s,0,out));s.color_f1=true;assert(fresh.render(s,0,other));assert(!memcmp(&out,&other,sizeof out));s.width=320;
 // Short palette and bad indices must not read past the advertised palette.
 uint8_t tiny[3]={0,0,0};s.palette=tiny;s.colors=1;memset(pixels,255,sizeof pixels);
 assert(cv.render(s,0,out,nullptr,&guarded.cache));for(const auto &c:out.cells)for(unsigned y=0;y<8;y++)assert(c[y]==0);
 s.palette=pal;s.colors=256;
 for(unsigned i=0;i<256;i++)pal[i*3]=pal[i*3+1]=pal[i*3+2]=i;
 for(unsigned i=0;i<64000;i++)pixels[i]=i%256;
 assert(cv.render(s,0,out,nullptr,&guarded.cache));
 for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++){const unsigned c=shown(out,x,y);assert(c==0||c==1||c==11||c==12||c==15);}
 s.context=pixels;s.pixels=nullptr;s.read_pixel=[](void *p,uint16_t x,uint16_t y){return static_cast<uint8_t *>(p)[y*320+x];};
 assert(cv.render(s,0,other,nullptr,&guarded.cache));assert(!memcmp(&out,&other,sizeof out));s.pixels=pixels;s.read_pixel=nullptr;
 // Palette colors remain solid in the protected status band, no exposure.
 const uint8_t green[3]={148,224,137};memcpy(pal,green,3);memset(pixels,0,sizeof pixels);s.solid_from_y=168;
 assert(cv.render(s,0,out,nullptr,&guarded.cache));for(unsigned y=168;y<200;y++)for(unsigned x=0;x<320;x++)assert(shown(out,x,y)==13);
 for(int i=1;i<argc;i++){
  std::ifstream f(argv[i],std::ios::binary);std::vector<uint8_t> data{std::istreambuf_iterator<char>(f),{}};assert(data.size()==64768);
  s.pixels=data.data();s.palette=data.data()+64000;
  assert(cv.render(s,0,out,nullptr,&guarded.cache));assert(fresh.render(s,0,other));assert(!memcmp(&out,&other,sizeof out));verify();
 }
 printf("PASS Color F1: legal/stable cells, cached=fresh, mode isolation, short palettes, raster input, solid status; converter %zu, cache %zu bytes\n",sizeof cv,sizeof(ColorF1Cache));
}
