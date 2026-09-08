// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include <string.h>

namespace mpe_video {
// Optional larger workspace retains exact images of both C64 banks. Older
// modules lending the ABI's 24 KiB workspace retain the full-upload fallback.
constexpr unsigned DeltaWorkspaceBytes=36864;
// Bounded real-time companion to the exhaustive reference converter. All
// scaling/palette decisions live in firmware; producers submit native pixels.
struct LiveFrame {
    uint8_t cells[1000][10];
    uint8_t split[25];
    uint32_t mask;
    uint8_t mode;
    uint8_t background;
    uint8_t overlays; // Uses existing struct padding; second plane is sprites.
    uint8_t multicolor; // Crop F3; fills the last padding byte, no size growth.
};
struct IndexedSource {
    const uint8_t *pixels,*palette;
    uint16_t width,height,stride,colors;
    uint16_t geometry=0; // bit 0: centered native height; bit 1: 2x pixel width
    uint8_t (*read_pixel)(void *,uint16_t,uint16_t)=nullptr;
    void *context=nullptr;
    uint16_t crop_x=0,crop_y=0;
    uint16_t background_index=256; // Internal crop hint; 256 keeps legacy choice.
};
class LiveConverter {
    uint8_t map_[256];
    uint32_t distance_[16][16];
    void overlay(const IndexedSource &s,LiveFrame &out,bool nativeWidth) const;
    static const uint8_t *palette();
    struct Pair {uint8_t a,b;};
    static Pair pair(const uint8_t *hist) {
        uint8_t a=0,b=0;
        for(uint8_t i=1;i<16;i++)if(hist[i]>hist[a])a=i;
        b=a;
        for(uint8_t i=0;i<16;i++)if(i!=a&&hist[i]&&(b==a||hist[i]>hist[b]))b=i;
        return {a,b};
    }
    uint32_t error(const uint8_t *hist,Pair p) const {
        uint32_t e=0;for(unsigned i=0;i<16;i++)e+=hist[i]*(distance_[i][p.a]<distance_[i][p.b]?distance_[i][p.a]:distance_[i][p.b]);return e;
    }
    // F5 only: consider the color suffering the greatest weighted loss.
    // Test just its two substitutions, never an exhaustive palette search.
    // Exact regions return immediately; ties preserve the legacy bit encoding.
    Pair detailPair(const uint8_t *hist,Pair best,unsigned samples) const {
        if(hist[best.a]+(best.a==best.b?0:hist[best.b])==samples)return best;
        const Pair original=best;uint32_t lowest=0,worst=0;uint8_t detail=best.b;
        for(uint8_t c=0;c<16;c++)if(hist[c]){
            const auto a=distance_[c][best.a],b=distance_[c][best.b];
            const auto loss=hist[c]*(a<b?a:b);lowest+=loss;
            if(loss>worst){worst=loss;detail=c;}
        }
        if(!worst)return best;
        uint32_t keepA=0,keepB=0;
        for(uint8_t c=0;c<16;c++)if(hist[c]){
            const auto a=distance_[c][original.a],b=distance_[c][original.b],d=distance_[c][detail];
            keepA+=hist[c]*(a<d?a:d);keepB+=hist[c]*(b<d?b:d);
        }
        if(keepA<lowest){best={original.a,detail};lowest=keepA;}
        if(keepB<lowest)best={detail,original.b};
        return best;
    }
    void encode(const uint8_t *pixels,uint8_t first,uint8_t end,Pair p,uint8_t *out) const {
        for(unsigned y=first;y<end;y++){out[y]=0;for(unsigned x=0;x<8;x++)
            if(distance_[pixels[y*8+x]][p.b]<distance_[pixels[y*8+x]][p.a])out[y]|=0x80>>x;}
    }
    void samples(const IndexedSource &s,unsigned cell,uint8_t *p,bool nativeWidth,bool colorMode) const {
        const unsigned cx=(cell%40)*8,cy=(cell/40)*8;
        const unsigned scale=(s.geometry&2)?2:1;
        const unsigned extent=s.width*scale;
        const unsigned left=nativeWidth?(320-extent)/2:0;
        const bool nativeHeight=(s.geometry&1)&&s.height<200;
        const unsigned top=nativeHeight?(200-s.height)/2:0;
        for(unsigned y=0;y<8;y++){const unsigned dy=cy+y;
            const bool margin=nativeHeight&&(dy<top||dy>=top+s.height);
            const unsigned sy=nativeHeight?dy-top:((2*dy+1)*s.height)/400;
            for(unsigned x=0;x<8;x++){
                const unsigned dx=cx+x;
                // Padding is C64 black, independent of the source palette.
                if(margin||(nativeWidth&&(dx<left||dx>=left+extent))){p[y*8+x]=0;continue;}
                const unsigned sx=nativeWidth?(dx-left)/scale:((2*dx+1)*s.width)/640;
                auto read=[&](unsigned x){return s.read_pixel?s.read_pixel(s.context,x,sy):s.pixels[sy*s.stride+x];};
                auto index=read(sx);
                if((s.geometry&8)&&!nativeWidth){
                    const unsigned last=colorMode?(dx|1):dx,first=colorMode?(dx&~1u):dx;
                    for(unsigned x=first*s.width/320;x<(last+1)*s.width/320;x++){
                        const auto next=read(x);if(next>index)index=next;
                    }
                }
                p[y*8+x]=map_[index];}}
    }
public:
    // mode: 0 ordinary multicolor; 1 Auto8; 2 Enhanced25; 3 Sharp.
    // Enhanced25/Sharp center narrower sources; all modes fit height.
    bool render(const IndexedSource &s,uint8_t mode,LiveFrame &out,const LiveFrame *previous=nullptr);
};
}
