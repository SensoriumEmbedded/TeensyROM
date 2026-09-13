// Exact full-height 8x2 output and changed-source conversion; no hardware FPS claim.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "../video/mpe_video_live.cpp"
using namespace mpe_video;
static uint8_t pixels[320*200],rgb[16][3]={
    {0,0,0},{255,255,255},{136,57,50},{103,182,189},
    {139,63,150},{85,160,73},{64,49,141},{191,206,114},
    {139,84,41},{87,66,0},{184,105,98},{80,80,80},
    {120,120,120},{148,224,137},{120,105,196},{159,159,159}};
struct Reader {unsigned calls=0,start=0;};
static uint8_t readPixel(void *p,uint16_t x,uint16_t y){
    auto &r=*static_cast<Reader *>(p);++r.calls;return pixels[y*320+(x+r.start)%320];
}
static uint8_t readMonochrome(void *p,uint16_t x,uint16_t){
    ++static_cast<Reader *>(p)->calls;return x%10==0||x%10==3;
}
static uint8_t displayed(const FullFrame &f,unsigned x,unsigned y){
    const unsigned cell=y/8*40+x/8,plane=(y&7)/2;
    const auto c=f.frame.cells[cell];const auto a=plane<2?c[8+plane]:f.extra[plane-2][cell];
    return c[y&7]&(128>>(x&7))?a>>4:a&15;
}
static void samePicture(const FullFrame &a,const FullFrame &b){
    assert(!memcmp(&a.frame,&b.frame,sizeof a.frame));assert(!memcmp(a.extra,b.extra,sizeof a.extra));
}
static unsigned sourceX(unsigned x){return ((x-24)*320+160)/296;}
static void checkPixels(const FullFrame &f){
    for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++)
        assert(displayed(f,x,y)==(x<24?0:pixels[y*320+sourceX(x)]));
}
int main(){
    static_assert(sizeof(LiveConverter)==1280,"Preserve existing center/ordinary workspace sizes");
    static_assert(sizeof(FullFrame)+sizeof(LiveConverter)+4096<=19456,"Full F5 fits the 19 KiB loan");
    LiveConverter converter,referenceConverter;FullFrame frame{},reference{};Reader reader;
    uint8_t dirty[125]{};IndexedSource source{nullptr,&rgb[0][0],320,200,320,16,0,readPixel,&reader};
    source.dirty_cells=dirty;bool changed=false;
    auto render=[&](unsigned reads){reader.calls=0;assert(converter.renderFull(source,frame,&changed));assert(reader.calls==reads);};
    auto compare=[&](){auto full=source;full.dirty_cells=nullptr;assert(referenceConverter.renderFull(full,reference));samePicture(frame,reference);};
    for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++)
        pixels[y*320+x]=uint8_t((y>=196?2:(y&7)/2)*2+(x&1));
    // The whole source extent fits beyond the black 24-pixel margin. A clear
    // source map never skips the first picture, and margin pixels read no VRAM.
    frame.invalidate();render(59200);assert(changed);compare();checkPixels(frame);
    assert(sourceX(24)==0&&sourceX(319)==319);
    assert(frame.frame.mask==(1u<<25)-1);for(auto split:frame.frame.split)assert(split==2);
    // The bottom four lines share one pair after fitting, preserving both
    // source ends instead of dropping bottom rows or introducing another fetch.
    for(unsigned y=196;y<200;y++)for(unsigned x=0;x<320;x++)pixels[y*320+x]=y<198?6:1;
    memset(dirty+120,0xff,5);render(37*64);assert(changed);compare();checkPixels(frame);
    for(unsigned c=963;c<1000;c++)assert(frame.extra[0][c]==frame.extra[1][c]);
    memset(dirty,0,sizeof dirty);
    memset(frame.dirty,0,sizeof frame.dirty);render(0);assert(!changed);
    // Every logical source column can straddle physical output cells after
    // fitting. Exhaustively exercise their dirty footprints, including both
    // edge columns, and exact bank dirtiness for changes and reversions.
    memset(pixels,0,sizeof pixels);memset(dirty,0xff,sizeof dirty);render(59200);compare();
    for(unsigned column=0;column<40;column++){
        bool touched[40]{};unsigned count=0;
        for(unsigned x=24;x<320;x++)if(sourceX(x)/8==column)touched[x/8]=true;
        for(bool touch:touched)count+=touch;
        assert(count>=1&&count<=2);
        memset(dirty,0,sizeof dirty);const unsigned cell=10*40+column;dirty[cell/8]|=1u<<(cell&7);
        for(unsigned value:{1u,0u}){
            memset(frame.dirty,0,sizeof frame.dirty);
            for(unsigned y=80;y<88;y++)memset(pixels+y*320+column*8,value,8);
            render(count*64);assert(changed);compare();checkPixels(frame);
            for(unsigned bank=0;bank<2;bank++)for(unsigned c=0;c<1000;c++)
                assert(frame.changed(bank,c)==(c/40==10&&touched[c%40]));
            memset(frame.dirty,0,sizeof frame.dirty);render(count*64);assert(!changed);
        }
        memset(dirty,0,sizeof dirty);render(0);assert(!changed);
    }
    // Interleaving an ordinary profile must not leave its palette lookup in
    // the full profile's converter. The full frame restores its exact map.
    uint8_t otherPalette[sizeof rgb]{};auto ordinary=source;ordinary.palette=otherPalette;
    LiveFrame ordinaryFrame{};assert(converter.render(ordinary,3,ordinaryFrame));
    dirty[0]=1;render(64);assert(!changed);compare();memset(dirty,0,sizeof dirty);
    // Exact palette contents, rather than pointer identity, invalidate cache.
    const auto red=rgb[1][0];rgb[1][0]=0;render(59200);assert(changed);compare();
    render(0);assert(!changed);rgb[1][0]=red;render(59200);compare();
    // Mapping policy and source dimensions are also part of the cache key.
    source.geometry=32;render(59200);assert(changed);compare();
    source.width=160;source.geometry=34;render(59200);assert(changed);compare();
    source.height=100;source.geometry=35;render(29600);assert(changed);compare();
    source.width=320;source.height=200;source.geometry=0;render(59200);compare();
    // Display-start changes occur inside the producer's context; null hints
    // explicitly invalidate them even though geometry and pointers are stable.
    pixels[13]=1;reader.start=13;source.dirty_cells=nullptr;render(59200);assert(changed);compare();
    source.dirty_cells=dirty;render(0);assert(!changed);
    frame.invalidate();render(59200);assert(changed);compare();
    // Palette counts and source identity invalidate otherwise-empty hints.
    source.colors=8;render(59200);assert(changed);compare();
    source.colors=16;Reader second;source.context=&second;
    assert(converter.renderFull(source,frame,&changed));assert(second.calls==59200&&changed);compare();
    source.width=0;assert(!converter.renderFull(source,frame,&changed)&&!changed);
    // Preserve DOS's other native widths: mode 8 doubles 160 source pixels
    // before fitting, while mode 6 retains either monochrome source bit in
    // each logical pair before fitting 640 pixels. Neither path crops an end.
    for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++)pixels[y*320+x]=x&1;
    reader.start=0;source.context=&reader;source.width=160;source.colors=2;
    source.geometry=2;source.dirty_cells=nullptr;render(59200);
    for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++)
        assert(displayed(frame,x,y)==(x<24?0:((sourceX(x)/2)&1)));
    Reader mono;source.width=640;source.geometry=8;source.read_pixel=readMonochrome;source.context=&mono;
    assert(converter.renderFull(source,frame,&changed)&&changed);assert(mono.calls==59200*3);
    for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++)
        assert(displayed(frame,x,y)==(x<24?0:sourceX(x)%5<2));
    printf("PASS: full source fitted to 296x200 beyond black margin, exact endpoints, bottom-four-line 8x4 fallback, all source dirty footprints/reversions, zero unchanged reads, palette/geometry/start invalidation; FullFrame=%zu converter=%zu workspace=%zu bytes\n",
           sizeof(FullFrame),sizeof(LiveConverter),sizeof(FullFrame)+sizeof(LiveConverter)+4096);
}
