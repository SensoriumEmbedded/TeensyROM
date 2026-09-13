// Source-level F5 quality and producer compatibility; not a hardware FPS test.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "../video/mpe_video_live.cpp"
using namespace mpe_video;
static const uint8_t rgb[16][3]={{0,0,0},{255,255,255},{136,57,50},{103,182,189},
 {139,63,150},{85,160,73},{64,49,141},{191,206,114},{139,84,41},{87,66,0},
 {184,105,98},{80,80,80},{120,120,120},{148,224,137},{120,105,196},{159,159,159}};
static uint8_t pixels[320*240];
static uint8_t displayed(const LiveFrame &f,unsigned x,unsigned y){
 const auto c=f.cells[y/8*40+x/8];
 const auto a=c[(f.mask&(1u<<(y/8)))&&y%8>=f.split[y/8]?9:8];
 return c[y%8]&(128>>(x%8))?a>>4:a&15;
}
static unsigned distance(uint8_t a,uint8_t b){unsigned n=0;for(unsigned c=0;c<3;c++){int d=rgb[a][c]-rgb[b][c];n+=d*d;}return n;}
static uint8_t readPixel(void *p,uint16_t x,uint16_t y){return pixels[y*static_cast<IndexedSource *>(p)->stride+x];}
int main(){
 LiveConverter converter;LiveFrame f5{},f7{};
 IndexedSource source{pixels,&rgb[0][0],320,200,320,16,64};
 // A three-pixel white highlight competes with two similar blue shades in
 // each half-cell. It must survive, without a third attribute or new badline.
 for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++){
  const unsigned n=(y%4)*8+x%8;pixels[y*320+x]=n<16?6:n<29?14:1;
 }
 assert(converter.render(source,2,f5));assert(converter.render(source,3,f7));
 unsigned long long enhancedError=0,sharpError=0;unsigned highlights=0;
 for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++){
  const auto original=pixels[y*320+x],a=displayed(f5,x,y),b=displayed(f7,x,y);
  enhancedError+=distance(original,a);sharpError+=distance(original,b);
  if(original==1){assert(a==1);assert(b!=1);highlights++;}
 }
 assert(highlights==6000&&enhancedError<sharpError);
 assert(sizeof(f5.cells)==10000&&sizeof(f5)==10036);
 for(auto split:f5.split)assert(split==4);
 const auto saved=f5;assert(converter.render(source,2,f5,&f5));assert(!memcmp(&saved,&f5,sizeof f5));
 // Scenery already expressible with two colors keeps every source pixel.
 for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++)pixels[y*320+x]=(x+y)&1?6:14;
 for(uint16_t geometry:{0,64}){source.geometry=geometry;assert(converter.render(source,2,f5));
  for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++)assert(displayed(f5,x,y)==pixels[y*320+x]);}
 // Actual producer shapes/hints: NES, GB/GBC, Doom (fixed/adaptive), DOS.
 // The byte and live-reader ABI paths must produce exactly the same picture.
 struct Shape{uint16_t width,height,geometry;};
 for(const auto shape:{Shape{256,240,0},Shape{160,144,3},Shape{320,200,64},Shape{320,200,0},Shape{320,200,32}}){
  source.width=source.stride=shape.width;source.height=shape.height;source.geometry=shape.geometry;
  for(unsigned y=0;y<shape.height;y++)for(unsigned x=0;x<shape.width;x++)pixels[y*shape.width+x]=uint8_t(((x/3)^(y/5))&15);
  LiveFrame bytes{},raster{};assert(converter.render(source,2,bytes));
  auto reader=source;reader.pixels=nullptr;reader.read_pixel=readPixel;reader.context=&source;
  assert(converter.render(reader,2,raster));assert(!memcmp(&bytes,&raster,sizeof bytes));
 }
 printf("PASS: F5 preserves 6000 minority highlight pixels; error %llu -> %llu; exact two-color scenery, immutable repeat, unchanged 10000-byte picture, NES/GB/Doom/DOS byte and raster geometry\n",sharpError,enhancedError);
}
