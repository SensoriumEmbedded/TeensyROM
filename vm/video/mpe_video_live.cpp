// SPDX-License-Identifier: MIT
#include "mpe_video_live.h"
#include "mpe_video_sprites.h"
#ifndef MPE_VIDEO_CODE
#define MPE_VIDEO_CODE
#endif
#include "mpe_video_color_f1.cpp"
namespace mpe_video {
MPE_VIDEO_CODE const uint8_t *LiveConverter::palette(){
    static const uint8_t rgb[16][3]={{0,0,0},{255,255,255},{136,57,50},{103,182,189},{139,63,150},{85,160,73},{64,49,141},{191,206,114},
        {139,84,41},{87,66,0},{184,105,98},{80,80,80},{120,120,120},{148,224,137},{120,105,196},{159,159,159}};
    return &rgb[0][0];
}
MPE_VIDEO_CODE void LiveConverter::prepare(const IndexedSource &s){
    const auto rgb=palette();
    for(unsigned i=0;i<16;i++)for(unsigned j=0;j<16;j++){uint32_t e=0;for(unsigned c=0;c<3;c++){int d=int(rgb[i*3+c])-rgb[j*3+c];e+=d*d;}distance_[i][j]=e;}
    for(unsigned i=0;i<256;i++){
        uint32_t best=~0u;map_[i]=0;if(i>=s.colors)continue;
        for(unsigned j=0;j<16;j++){uint32_t e=0;for(unsigned c=0;c<3;c++){int d=int(s.palette[i*3+c])-rgb[j*3+c];e+=d*d;}
            if(e<best){best=e;map_[i]=j;}}
        if(s.geometry&32){
            // Preserve DOS's established RGBI mapping. Only exact RGBI
            // entries take this path; arbitrary RGB still uses nearest color.
            static const uint8_t vic[16]={0,6,5,3,2,4,8,15,11,14,13,3,10,4,7,1};
            for(unsigned c=0;c<16;c++){
                const unsigned light=(c&8)?85:0;
                if(s.palette[i*3]==((c&4)?170:0)+light&&s.palette[i*3+1]==(c==6?85:((c&2)?170:0)+light)&&s.palette[i*3+2]==((c&1)?170:0)+light){map_[i]=vic[c];break;}
            }
        }
    }
    if(s.geometry&256)for(unsigned i=0;i<64;i++)map_[i|64]=map_[i];
}
MPE_VIDEO_CODE bool LiveConverter::render(const IndexedSource &s,uint8_t mode,LiveFrame &out,const LiveFrame *previous,ColorF1Cache *cache){
    if((!s.pixels&&!s.read_pixel)||!s.palette||!s.width||!s.height||s.width>1024||s.height>1024||(!s.read_pixel&&s.stride<s.width)||!s.colors||s.colors>256||mode>3)return false;
    if((s.geometry&~1019)||((s.geometry&256)&&s.colors>64))return false;
    if(mode==0&&s.color_f1&&s.width==320&&s.height==200){renderColorF1(s,out,cache);return true;}
    if(mode==1&&(s.geometry&512)){
        if(s.width<160||s.height<200||s.crop_x>s.width-160||s.crop_y>s.height-200)return false;
        auto crop=s;crop.width=160;crop.height=200;crop.geometry=s.geometry&~512;
        // Spend the shared color on the scene, rather than reserving black
        // when a bright sky plus three actor colors need all four slots.
        // Sample the full source on a fixed 8-pixel grid (960 NES reads), so
        // camera movement alone cannot change the shared color selection.
        uint16_t histogram[256]{};
        for(unsigned y=0;y<s.height;y+=8)for(unsigned x=0;x<s.width;x+=8){
            auto index=s.read_pixel?s.read_pixel(s.context,x,y):s.pixels[y*s.stride+x];
            if(s.geometry&256)index&=63;if(index<s.colors)++histogram[index];
        }
        crop.background_index=0;
        for(unsigned i=1;i<s.colors;i++)if(histogram[i]>histogram[crop.background_index])crop.background_index=i;
        struct Reader {const IndexedSource *source;};Reader reader{&s};
        if(s.read_pixel){crop.context=&reader;crop.read_pixel=[](void *p,uint16_t x,uint16_t y){
            const auto &source=*static_cast<Reader *>(p)->source;
            return source.read_pixel(source.context,x+source.crop_x,y+source.crop_y);
        };}else crop.pixels=s.pixels+s.crop_y*s.stride+s.crop_x;
        if(!render(crop,0,out))return false;
        out.mode=1;out.multicolor=1;return true;
    }
    out.multicolor=0;
    const bool spriteF5=mode==2&&(s.geometry&128);
    const bool nativeWidth=(mode==2||mode==3)&&(unsigned(s.width)*((s.geometry&2)?2:1)<=320);
    const uint32_t previousMask=previous?previous->mask:0;uint8_t previousSplit[25]{};
    if(previous)memcpy(previousSplit,previous->split,25);
    prepare(s);
    out.overlays=0;
    out.background=mode==0?(s.background_index<s.colors?map_[s.background_index]:(s.geometry&16)?map_[0]:0):0;
    const bool stable=(s.geometry&64)&&mode==2&&!spriteF5;
    uint32_t base[25]{},cost[25][7]{};uint8_t bestSplit[25]{};uint32_t gain[25]{};
    for(unsigned cell=0;cell<1000;cell++){
        uint8_t p[64],hist[16]{};samples(s,cell,p,nativeWidth,mode==0);auto dst=out.cells[cell];
        if(stable){
            uint8_t bottom[16]{};
            for(unsigned i=0;i<32;i++)hist[p[i]]++;
            for(unsigned i=32;i<64;i++)bottom[p[i]]++;
            const auto a=detailPair(hist,pair(hist),32),b=detailPair(bottom,pair(bottom),32);
            encode(p,0,4,a,dst);encode(p,4,8,b,dst);
            dst[8]=(a.b<<4)|a.a;dst[9]=(b.b<<4)|b.a;continue;
        }
        if(mode==0){
            for(unsigned y=0;y<8;y++)for(unsigned x=0;x<4;x++)hist[p[y*8+x*2+1]]++;
            uint8_t col[4]={out.background,0,0,0};hist[out.background]=0;
            for(unsigned k=1;k<4;k++){col[k]=pair(hist).a;hist[col[k]]=0;}
            for(unsigned y=0;y<8;y++){dst[y]=0;for(unsigned x=0;x<4;x++){uint8_t v=p[y*8+x*2+1],b=0;
                for(unsigned k=1;k<4;k++)if(distance_[v][col[k]]<distance_[v][col[b]])b=k;dst[y]|=b<<(6-2*x);}}
            dst[8]=(col[1]<<4)|col[2];dst[9]=col[3];continue;
        }
        for(auto v:p)hist[v]++;const Pair full=pair(hist);
        const Pair shown=mode==2&&!spriteF5?detailPair(hist,full,64):full;
        encode(p,0,8,shown,dst);dst[8]=(shown.b<<4)|shown.a;dst[9]=dst[8];
        if(mode==3||spriteF5)continue;
        const auto fullError=error(hist,full);base[cell/40]+=fullError;
        // Exact cells contribute zero to every possible split. Avoid fourteen
        // redundant palette searches, paying for F5's detail work elsewhere.
        if(!fullError)continue;
        uint8_t top[16]{},bottom[16];memcpy(bottom,hist,16);
        for(unsigned split=1;split<8;split++){
            for(unsigned x=0;x<8;x++){auto v=p[(split-1)*8+x];top[v]++;bottom[v]--;}
            cost[cell/40][split-1]+=error(top,pair(top))+error(bottom,pair(bottom));
        }
    }
    out.mode=mode;out.mask=0;memset(out.split,0,sizeof out.split);
    if(spriteF5){overlay(s,out,nativeWidth);out.overlays=1;return true;}
    if(stable){out.mask=(1u<<25)-1;memset(out.split,4,sizeof out.split);return true;}
    if(mode==0||mode==3)return true;
    for(unsigned band=0;band<25;band++){
        unsigned split=0;for(unsigned k=1;k<7;k++)if(cost[band][k]<cost[band][split])split=k;
        // Keep an established split unless the new one improves error by 10%.
        if((previousMask&(1u<<band))&&previousSplit[band]>=1&&previousSplit[band]<=7){
            unsigned old=previousSplit[band]-1;if(uint64_t(cost[band][old])*9<=uint64_t(cost[band][split])*10)split=old;
        }
        bestSplit[band]=split+1;
        // Ignore tiny improvements; this also avoids palette-plan churn.
        if(base[band]>cost[band][split]&&base[band]-cost[band][split]>base[band]/32+1024)gain[band]=base[band]-cost[band][split];
    }
    for(unsigned slot=0;slot<(mode==1?8u:25u);slot++){
        unsigned band=25;uint32_t score=0;
        for(unsigned b=0;b<25;b++)if(gain[b]&&!(out.mask&(1u<<b))){
            uint32_t g=gain[b];if(previousMask&(1u<<b))g+=g/8;
            if(g>score){score=g;band=b;}}
        if(band==25)break;out.mask|=1u<<band;out.split[band]=bestSplit[band];
    }
    for(unsigned cell=0;cell<1000;cell++)if(out.mask&(1u<<(cell/40))){
        uint8_t p[64],top[16]{},bottom[16]{};samples(s,cell,p,nativeWidth,false);const auto split=out.split[cell/40];
        for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++)(y<split?top:bottom)[p[y*8+x]]++;
        auto a=pair(top),b=pair(bottom);auto dst=out.cells[cell];
        if(mode==2){
            a=detailPair(top,a,split*8);b=detailPair(bottom,b,(8-split)*8);
            const Pair full{uint8_t(dst[8]&15),uint8_t(dst[8]>>4)};
            // A globally chosen band split need not improve every cell. Keep
            // its better unsplit pair when the local split would lose detail.
            if(error(top,a)+error(bottom,b)>error(top,full)+error(bottom,full))a=b=full;
        }
        encode(p,0,split,a,dst);encode(p,split,8,b,dst);
        dst[8]=(a.b<<4)|a.a;dst[9]=(b.b<<4)|b.a;
    }
    return true;
}
MPE_VIDEO_CODE bool LiveConverter::renderCenter(const IndexedSource &s,CenterFrame &out,unsigned first,unsigned rows){
    if((!s.pixels&&!s.read_pixel)||!s.palette||!s.width||!s.height||s.width>1024||s.height>1024||
       (!s.read_pixel&&s.stride<s.width)||!s.colors||s.colors>256||!rows||rows>9||first+rows>25||(s.geometry&~59))return false;
    prepare(s);
    renderQuad(s,out.frame,out.extra[0],out.extra[1],&out.dirty[0][0],first,rows,nullptr);
    return true;
}
MPE_VIDEO_CODE bool LiveConverter::renderFull(const IndexedSource &s,FullFrame &out,bool *changed){
    if(changed)*changed=false;
    if((!s.pixels&&!s.read_pixel)||!s.palette||!s.width||!s.height||s.width>1024||s.height>1024||
       (!s.read_pixel&&s.stride<s.width)||!s.colors||s.colors>256||(s.geometry&~59))return false;
    auto &cache=out.cache;const auto &old=cache.source;
    const bool paletteChanged=!cache.valid||cache.converter!=this||old.colors!=s.colors||
        ((old.geometry^s.geometry)&32)||memcmp(cache.palette,s.palette,s.colors*3);
    const bool sourceChanged=paletteChanged||old.pixels!=s.pixels||old.read_pixel!=s.read_pixel||old.context!=s.context||
        old.width!=s.width||old.height!=s.height||old.stride!=s.stride||old.geometry!=s.geometry||
        old.crop_x!=s.crop_x||old.crop_y!=s.crop_y||old.background_index!=s.background_index;
    if(paletteChanged){prepare(s);memcpy(cache.palette,s.palette,s.colors*3);memcpy(cache.map,map_,sizeof map_);}
    // Other rendering profiles may have used this converter since the last
    // full frame. Restore the small lookup instead of repeating RGB searches;
    // the C64-to-C64 distance table is independent of the source palette.
    else memcpy(map_,cache.map,sizeof map_);
    const bool outputChanged=renderQuad(s,out.frame,out.extra[0],out.extra[1],&out.dirty[0][0],0,25,
                                      sourceChanged?nullptr:s.dirty_cells);
    cache.source=s;cache.converter=this;cache.valid=true;
    if(changed)*changed=sourceChanged||outputChanged;
    return true;
}
MPE_VIDEO_CODE bool LiveConverter::renderQuad(const IndexedSource &s,LiveFrame &frame,uint8_t *extra0,uint8_t *extra1,
                                            uint8_t *dirty,unsigned first,unsigned rows,const uint8_t *sourceDirty){
    const bool nativeWidth=unsigned(s.width)*((s.geometry&2)?2:1)<=320;
    const bool fullFit=first==0&&rows==25;
    bool outputChanged=false;
    for(unsigned cell=0;cell<1000;cell++){
        const unsigned band=cell/40,local=cell-first*40;
        if(sourceDirty){
            if(fullFit){
                const unsigned x=(cell%40)*8;
                if(x<FullPictureLeft)continue; // constant black margin
                const unsigned from=band*40+fullSourceX(x)/8,to=band*40+fullSourceX(x+7)/8;
                bool changed=false;
                for(unsigned c=from;c<=to;c++)changed|=(sourceDirty[c/8]&(1u<<(c&7)))!=0;
                if(!changed)continue;
            }else if(!(sourceDirty[cell/8]&(1u<<(cell&7))))continue;
        }
        const bool enhanced=band>=first&&band<first+rows;
        // With the normal 200-line picture origin, the VIC cannot create the
        // final forced badline at raster 249. Preserve the last four source
        // rows with one 8x4 pair instead of hiding or dropping those pixels.
        const bool bottomPair=fullFit&&band==24;
        uint8_t p[64],next[10]{},extra[2]{};samples(s,cell,p,nativeWidth,false,fullFit);
        for(unsigned plane=0;plane<(enhanced?(bottomPair?3u:4u):1u);plane++){
            const unsigned begin=enhanced?plane*2:0,end=enhanced&&!(bottomPair&&plane==2)?begin+2:8;
            uint8_t hist[16]{};for(unsigned i=begin*8;i<end*8;i++)++hist[p[i]];
            const auto choice=detailPair(hist,pair(hist),(end-begin)*8);
            encode(p,begin,end,choice,next);
            const uint8_t attribute=(choice.b<<4)|choice.a;
            if(plane<2)next[8+plane]=attribute;else extra[plane-2]=attribute;
        }
        if(bottomPair)extra[1]=extra[0];
        if(!enhanced)next[9]=next[8];
        bool changed=memcmp(next,frame.cells[cell],10)!=0;
        if(enhanced)for(unsigned i=0;i<2;i++){
            auto plane=i?extra1:extra0;
            changed|=extra[i]!=plane[local];plane[local]=extra[i];
        }
        if(changed){outputChanged=true;for(unsigned bank=0;bank<2;bank++)dirty[bank*125+cell/8]|=1u<<(cell&7);}
        memcpy(frame.cells[cell],next,10);
    }
    frame.mode=2;frame.background=frame.overlays=frame.multicolor=0;
    frame.mask=((1u<<rows)-1)<<first;memset(frame.split,2,sizeof frame.split);
    return outputChanged;
}
MPE_VIDEO_CODE void LiveConverter::overlay(const IndexedSource &s,LiveFrame &out,bool nativeWidth) const {
    const unsigned scale=(s.geometry&2)?2:1,extent=s.width*scale,left=nativeWidth?(320-extent)/2:0;
    const bool nativeHeight=(s.geometry&1)&&s.height<200;
    const unsigned top=nativeHeight?(200-s.height)/2:0;
    // Encode C64 color plus optional actor hint in one reader result, avoiding
    // a second VRAM callback. Padding is always black and never prioritized.
    auto source=[&](unsigned i)->uint8_t{
        const unsigned x=i%320,y=i/320;
        if((nativeWidth&&(x<left||x>=left+extent))||(nativeHeight&&(y<top||y>=top+s.height)))return 0;
        const unsigned sy=nativeHeight?y-top:((2*y+1)*s.height)/400;
        const unsigned sx=nativeWidth?(x-left)/scale:((2*x+1)*s.width)/640;
        auto read=[&](unsigned xx){return s.read_pixel?s.read_pixel(s.context,xx,sy):s.pixels[sy*s.stride+xx];};
        auto index=read(sx);
        if((s.geometry&8)&&!nativeWidth)for(unsigned xx=x*s.width/320;xx<(x+1)*s.width/320;xx++){
            const auto next=read(xx);if(next>index)index=next;
        }
        return map_[index]|((s.geometry&256)&&(index&64)?16:0);
    };
    auto base=[&](unsigned i){const unsigned x=i%320,y=i/320;const auto c=out.cells[y/8*40+x/8];
        return uint8_t((c[y%8]&(128>>(x%8)))?c[8]>>4:c[8]&15);};
    const auto plan=sprites::select(source,base,[&](uint8_t a,uint8_t b){return distance_[a][b];});
    sprites::pack(plan,[&](unsigned i,uint8_t b){out.cells[i][9]=b;});
}
}
