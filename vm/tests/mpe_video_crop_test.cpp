// Native-pixel crop, camera behavior, reader parity and untouched selectors.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <filesystem>
#include "../video/mpe_video_live.cpp"
#include "../video/mpe_video_camera.h"
using namespace mpe_video;
static const uint8_t rgb[][3]={{0,0,0},{255,255,255},{136,57,50},{103,182,189},{139,63,150}};
static uint8_t pixels[256*240];
static uint8_t read(void *p,uint16_t x,uint16_t y){assert(x<256&&y<240);return static_cast<uint8_t *>(p)[y*256+x];}
static uint8_t shown(const LiveFrame &f,unsigned x,unsigned y){
 const auto c=f.cells[y/8*40+x/4];const uint8_t colors[]={f.background,uint8_t(c[8]>>4),uint8_t(c[8]&15),c[9]};
 return colors[(c[y%8]>>(6-2*(x%4)))&3];
}
static void save(const char *directory,const char *name,const LiveFrame &f){
 std::filesystem::create_directories(directory);const auto path=std::filesystem::path(directory)/name;
 uint8_t packed[10000],expected[64000];
 for(unsigned i=0;i<1000;i++){memcpy(packed+i*8,f.cells[i],8);packed[8000+i]=f.cells[i][8];packed[9000+i]=f.cells[i][9];}
 for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++)expected[y*320+x]=shown(f,x/2,y);
 std::ofstream b(path.string()+".bin",std::ios::binary);b.write((const char *)packed,sizeof packed);assert(b);
 std::ofstream e(path.string()+".expected",std::ios::binary);e.write((const char *)expected,sizeof expected);assert(e);
 std::ofstream bg(path.string()+".background",std::ios::binary);bg.put(f.background);assert(bg);
}
int main(int argc,char **argv){
 CropCamera camera;uint32_t now=0xfffffff0;
 camera.input(1,0,now);camera.position(256,240,now);assert(camera.x==48&&camera.y==20);
 camera.input(1,9,now);camera.position(256,240,now);assert(camera.x==52&&camera.y==16);
 now+=CropCamera::RepeatUs-1;camera.position(256,240,now);assert(camera.x==52&&camera.y==16);
 now++;camera.position(256,240,now);assert(camera.x==56&&camera.y==12); // clock wrap
 camera.input(1,15,now);now+=CropCamera::RepeatUs;camera.position(256,240,now);assert(camera.x==56&&camera.y==12);
 camera.input(1,6,now);for(unsigned n=0;n<30;n++){now+=CropCamera::RepeatUs;camera.position(256,240,now);}assert(camera.x==0&&camera.y==40);
 camera.input(1,9,now);for(unsigned n=0;n<30;n++){now+=CropCamera::RepeatUs;camera.position(256,240,now);}assert(camera.x==96&&camera.y==0);
 camera.input(1,0,now);now+=1000000;camera.position(256,240,now);assert(camera.x==96&&camera.y==0);
 camera.input(3,15,now);assert(!camera.held);camera.input(1,0,now);camera.position(256,240,now);assert(camera.x==48&&camera.y==20);
 // Every native pixel differs from a downsampled view. Include sprite tags
 // throughout; they are metadata, never an extra palette color in F3.
 for(unsigned y=0;y<240;y++)for(unsigned x=0;x<256;x++)pixels[y*256+x]=64|((x+(y/3)+(x/11)+(y/17))%4);
 IndexedSource s{pixels,&rgb[0][0],256,240,256,4,128|256|512};
 LiveConverter cv;LiveFrame f{},r{},legacy{},opted{};
 for(const auto &pos:{std::pair<unsigned,unsigned>{48,20},{0,0},{96,40},{0,40},{96,0}}){
  s.crop_x=pos.first;s.crop_y=pos.second;assert(cv.render(s,1,f)&&f.mode==1&&f.multicolor&&!f.mask&&!f.overlays);
  for(unsigned y=0;y<200;y++)for(unsigned x=0;x<160;x++)assert(shown(f,x,y)==(pixels[(y+s.crop_y)*256+x+s.crop_x]&63));
  auto callback=s;callback.pixels=nullptr;callback.context=pixels;callback.read_pixel=read;
  assert(cv.render(callback,1,r)&&!memcmp(&f,&r,sizeof f));
  if(argc==2&&pos==std::pair<unsigned,unsigned>{48,20})save(argv[1],"center",f);
  if(argc==2&&pos==std::pair<unsigned,unsigned>{96,40})save(argv[1],"corner",f);
 }
 auto old=s;old.geometry&=~512;
 for(uint8_t mode:{0,2,3}){assert(cv.render(old,mode,legacy)&&cv.render(s,mode,opted));assert(!memcmp(&legacy,&opted,sizeof legacy));}
 if(argc==2){assert(cv.render(s,0,f));save(argv[1],"f1",f);}
 // Nonblack scenery plus three actor colors must retain all four colors.
 // Panning alone must not change the global background.
 memset(pixels,64|3,sizeof pixels);s.colors=5;
 for(unsigned y=80;y<112;y++)for(unsigned x=100;x<120;x++)pixels[y*256+x]=64|(1+(x+y)%4);
 for(unsigned x0:{0u,48u,96u}){
  s.crop_x=x0;s.crop_y=20;assert(cv.render(s,1,f)&&f.background==3);
  for(unsigned y=0;y<200;y++)for(unsigned x=0;x<160;x++)assert(shown(f,x,y)==(pixels[(y+s.crop_y)*256+x+s.crop_x]&63));
 }
 if(argc==2)save(argv[1],"actor",f);
 s.crop_x=97;assert(!cv.render(s,1,f));s.crop_x=0;s.crop_y=41;assert(!cv.render(s,1,f));s.crop_y=0;
 s.width=159;assert(!cv.render(s,1,f));s.width=256;s.height=199;assert(!cv.render(s,1,f));
 static_assert(sizeof(LiveFrame)==10036,"Crop must not grow image workspace");
 puts("PASS: exact 160x200 pixels at center/all corners, tagged/reader parity, unchanged F1/F5/F7, camera repeat/release/opposites/clamps/reentry/clock wrap");
}
