// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mean Hamster Software.
// Palette fitting runs only on a palette change; frame work is indexed
// sampling, a small occupied-color fit and ordinary multicolor packing.
namespace mpe_video { namespace color_f1 {
struct RGB {int r,g,b;};
static inline unsigned error(RGB a,RGB b){const int r=a.r-b.r,g=a.g-b.g,blue=a.b-b.b;return unsigned(r*r+g*g+blue*blue);}
static inline int expose(int v){const int lifted=(v*480+128)/256;return lifted>255?255:lifted;}
static inline unsigned rank(unsigned x,unsigned y){return (((x^y)&1)<<3)|((y&1)<<2)|((((x>>1)^(y>>1))&1)<<1)|((y>>1)&1);}
}
MPE_VIDEO_CODE void LiveConverter::renderColorF1(const IndexedSource &s,LiveFrame &out,ColorF1Cache *storage){
    // Direct converter users can omit a cache. The firmware always lends
    // one for this profile; there is no global or cross-VM palette state.
    ColorF1Cache local{};auto &cache=storage?*storage:local;
    if(!cache.ready||cache.owner!=this||cache.colors!=s.colors||memcmp(cache.palette,s.palette,s.colors*3)){
        auto plain=s;plain.geometry=0;prepare(plain);
        using namespace color_f1;
        RGB vic[16];const auto rgb=palette();
        for(unsigned c=0;c<16;c++)vic[c]={rgb[c*3],rgb[c*3+1],rgb[c*3+2]};
        for(unsigned index=0;index<256;index++){
            if(index>=s.colors){cache.pair[index]=cache.fraction[index]=0;continue;}
            const RGB raw{s.palette[index*3],s.palette[index*3+1],s.palette[index*3+2]};
            RGB p{expose(raw.r),expose(raw.g),expose(raw.b)};
            const int high=p.r>p.g?(p.r>p.b?p.r:p.b):(p.g>p.b?p.g:p.b);
            const int low=p.r<p.g?(p.r<p.b?p.r:p.b):(p.g<p.b?p.g:p.b);
            const bool neutral=high-low<=32;
            if(!neutral){const int lift=(high-low)/5;
                p.r=p.r+lift>255?255:p.r+lift;p.g=p.g+lift>255?255:p.g+lift;p.b=p.b+lift>255?255:p.b+lift;}
            unsigned allowed=neutral?((1u<<0)|(1u<<1)|(1u<<11)|(1u<<12)|(1u<<15)):65535;
            if(raw.g<=raw.r+16||raw.g<=raw.b+16)allowed&=~((1u<<5)|(1u<<13));
            if(raw.g*8<raw.r*7||raw.r<=raw.b+16||raw.g<=raw.b+16)allowed&=~(1u<<7);
            unsigned first=0,second=0,fraction=0,score=~0u;
            for(unsigned c=0;c<16;c++)if(allowed&(1u<<c)){
                const unsigned e=color_f1::error(p,vic[c]);if(e<score){score=e;first=second=c;}}
            for(unsigned a=0;a<16;a++)if(allowed&(1u<<a))for(unsigned b=a+1;b<16;b++)if(allowed&(1u<<b)){
                const int r=vic[b].r-vic[a].r,g=vic[b].g-vic[a].g,blue=vic[b].b-vic[a].b;
                const int length=r*r+g*g+blue*blue;
                const int projection=(p.r-vic[a].r)*r+(p.g-vic[a].g)*g+(p.b-vic[a].b)*blue;
                if(projection<=0||projection>=length)continue;
                const unsigned t=unsigned((projection*16+length/2)/length);if(t==0||t==16)continue;
                const RGB mix{(vic[a].r*int(16-t)+vic[b].r*int(t)+8)/16,
                    (vic[a].g*int(16-t)+vic[b].g*int(t)+8)/16,(vic[a].b*int(16-t)+vic[b].b*int(t)+8)/16};
                const unsigned e=color_f1::error(p,mix)+unsigned(length)*t*(16-t)/3200;
                if(e+12<score){score=e;first=a;second=b;fraction=t;}
            }
            cache.pair[index]=uint8_t((first<<4)|second);cache.fraction[index]=uint8_t((map_[index]<<4)|fraction);
        }
        memcpy(cache.palette,s.palette,s.colors*3);cache.colors=s.colors;cache.owner=this;cache.ready=true;
    }
    out.mode=out.mask=out.background=out.overlays=out.multicolor=0;memset(out.split,0,sizeof out.split);
    unsigned solid=s.solid_from_y;if(solid>200||(solid&7))solid=200;
    for(unsigned tile=0;tile<1000;tile++){
        uint8_t samples[32],counts[16]{},map[16]{};
        const unsigned x0=(tile%40)*8,y0=(tile/40)*8;
        for(unsigned y=0;y<8;y++)for(unsigned x=0;x<4;x++){
            const auto index=s.read_pixel?s.read_pixel(s.context,x0+x*2+1,y0+y):s.pixels[(y0+y)*s.stride+x0+x*2+1];
            const auto pair=cache.pair[index];
            const uint8_t c=y0>=solid?(cache.fraction[index]>>4):color_f1::rank(x0/2+x,y0+y)<(cache.fraction[index]&15)?(pair&15):(pair>>4);
            samples[y*4+x]=c;++counts[c];
        }
        uint8_t selected[4]={0,0,0,0},present[15];unsigned used=0;
        for(unsigned c=1;c<16;c++)if(counts[c])present[used++]=uint8_t(c);
        if(used<=3){for(unsigned c=0;c<used;c++)selected[c+1]=present[c];}
        else {
            unsigned nearest[15];for(unsigned i=0;i<used;i++)nearest[i]=(distance_[present[i]][0]+3)/4;
            for(unsigned slot=1;slot<4;slot++){
                unsigned best=0,bestGain=0;
                for(unsigned c=0;c<used;c++){
                    unsigned gain=0;for(unsigned i=0;i<used;i++){
                        const unsigned next=(distance_[present[i]][present[c]]+3)/4;
                        if(next<nearest[i])gain+=unsigned(counts[present[i]])*(nearest[i]-next);}
                    if(gain>bestGain){best=c;bestGain=gain;}}
                selected[slot]=present[best];
                for(unsigned i=0;i<used;i++){const unsigned next=(distance_[present[i]][present[best]]+3)/4;if(next<nearest[i])nearest[i]=next;}
            }
        }
        if(selected[1]>selected[2]){auto t=selected[1];selected[1]=selected[2];selected[2]=t;}
        if(selected[2]>selected[3]){auto t=selected[2];selected[2]=selected[3];selected[3]=t;}
        if(selected[1]>selected[2]){auto t=selected[1];selected[1]=selected[2];selected[2]=t;}
        for(unsigned c=0;c<16;c++){
            unsigned best=0;for(unsigned slot=1;slot<4;slot++)if((distance_[c][selected[slot]]+3)/4<(distance_[c][selected[best]]+3)/4)best=slot;
            map[c]=uint8_t(best);
        }
        auto dst=out.cells[tile];dst[8]=uint8_t((selected[1]<<4)|selected[2]);dst[9]=selected[3];
        for(unsigned y=0;y<8;y++)dst[y]=uint8_t((map[samples[y*4]]<<6)|(map[samples[y*4+1]]<<4)|(map[samples[y*4+2]]<<2)|map[samples[y*4+3]]);
    }
}
}
