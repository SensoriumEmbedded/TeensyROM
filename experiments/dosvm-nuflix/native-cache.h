#pragma once
#include "native-display.h"

namespace dosvm_nuflix {
struct CacheStats {
    FitStats fit;
    unsigned examinedBands=0,unchangedBands=0,packedBands=0,deferredUnderlayUpdates=0;
    bool success=false;
};
class CachedConverter {
    static constexpr unsigned RawBytes=500,ScheduledBytes=800,SourceHashBytes=100;
    static uint8_t &storage(NativeDisplay &display,unsigned offset) {
        return offset<1194?display.fit.underlay[offset+6]:display.fit.bug[offset-1194+4];
    }
    static uint8_t nibble(const uint8_t *values,unsigned offset) { return (values[offset/2]>>((offset&1)*4))&15; }
    static void putNibble(uint8_t *values,unsigned offset,unsigned value) {
        const unsigned shift=(offset&1)*4;
        values[offset/2]=uint8_t((values[offset/2]&~(15u<<shift))|((value&15)<<shift));
    }
    static void encodeRaw(const NativeFit &fit,uint8_t *raw) {
        for(unsigned pair=0;pair<100;++pair){
            for(unsigned slot=0;slot<6;++slot)putNibble(raw,pair*10+slot,fit.underlay[pair*12+slot]);
            for(unsigned slot=0;slot<4;++slot)putNibble(raw,pair*10+6+slot,fit.bug[pair*4+slot]);
        }
    }
    static void restoreRaw(NativeFit &fit,const uint8_t *raw) {
        for(unsigned pair=0;pair<100;++pair){
            for(unsigned slot=0;slot<6;++slot)fit.underlay[pair*12+slot]=fit.underlay[pair*12+6+slot]=nibble(raw,pair*10+slot);
            for(unsigned slot=0;slot<4;++slot)fit.bug[pair*4+slot]=nibble(raw,pair*10+6+slot);
        }
    }
    static bool sameScheduledBand(const NativeFit &fit,const uint8_t *previous,unsigned band) {
        for(unsigned row=band*8;row<band*8+8;++row)for(unsigned slot=0;slot<6;++slot)
            if((fit.underlay[row*6+slot]&15)!=nibble(previous,row*6+slot))return false;
        for(unsigned pair=band*4;pair<band*4+4;++pair)for(unsigned slot=0;slot<4;++slot)
            if((fit.bug[pair*4+slot]&15)!=nibble(previous,1200+pair*4+slot))return false;
        const unsigned next=(band+1)*48+5;
        return band==24||(fit.underlay[next]&15)==nibble(previous,next);
    }
public:
    static constexpr unsigned StoredBytes=RawBytes+ScheduledBytes+SourceHashBytes;
    static void alignCode(NativeDisplay &display) {
        const unsigned shift=sizeof(display.code)-display.codeBytes;
        memmove(display.code+shift,display.code,display.codeBytes);memset(display.code,0,shift);
        display.codeBase=uint16_t(0x1000+shift);display.bankOperand=uint16_t(display.bankOperand+shift);
    }
    static NUFLIX_CODE CacheStats convert(const NativeConverter &colors,const NativeSource &source,NativeDisplay &display,
                                          bool ntsc,const uint8_t *slowValues,const uint8_t *hints,bool valid) {
        CacheStats stats;
        uint8_t raw[RawBytes]{},scheduled[ScheduledBytes]{},dirty[125];
        uint32_t sourceHashes[25]{};
        if(valid){
            for(unsigned offset=0;offset<RawBytes;++offset)raw[offset]=storage(display,offset);
            for(unsigned offset=0;offset<ScheduledBytes;++offset)scheduled[offset]=storage(display,RawBytes+offset);
            for(unsigned offset=0;offset<SourceHashBytes;++offset)reinterpret_cast<uint8_t *>(sourceHashes)[offset]=storage(display,RawBytes+ScheduledBytes+offset);
        }
        if(valid&&hints)memcpy(dirty,hints,sizeof(dirty));else memset(dirty,255,sizeof(dirty));
        for(unsigned band=0;band<25;++band){
            uint8_t any=0;for(unsigned column=0;column<5;++column)any|=dirty[band*5+column];
            if(!any){++stats.unchangedBands;continue;}
            ++stats.examinedBands;
            uint32_t hash=2166136261u;
            for(unsigned row=band*8;row<band*8+8;++row)for(unsigned column=0;column<320;++column)hash=(hash^(source.read(source.context,column,row)&15))*16777619u;
            if(valid&&hash==sourceHashes[band]){memset(dirty+band*5,0,5);++stats.unchangedBands;}
            sourceHashes[band]=hash;
        }
        if(valid&&stats.unchangedBands==25){stats.success=true;return stats;}
        if(valid)restoreRaw(display.fit,raw);
        stats.fit=colors.convert(source,display.fit,dirty);
        encodeRaw(display.fit,raw);
        DisplayConverter converter(colors,source);
        if(!converter.schedule(display,ntsc,slowValues))return stats;
        stats.deferredUnderlayUpdates=converter.deferredUnderlayUpdates;
        alignCode(display);
        uint32_t bands=0;
        for(unsigned band=0;band<25;++band){
            uint8_t any=0;for(unsigned column=0;column<5;++column)any|=dirty[band*5+column];
            if(!valid||any||!sameScheduledBand(display.fit,scheduled,band)){bands|=1u<<band;++stats.packedBands;}
        }
        converter.pack(display,nullptr,bands);
        for(unsigned offset=0;offset<1200;++offset)putNibble(scheduled,offset,display.fit.underlay[offset]);
        for(unsigned offset=0;offset<400;++offset)putNibble(scheduled,1200+offset,display.fit.bug[offset]);
        for(unsigned offset=0;offset<RawBytes;++offset)storage(display,offset)=raw[offset];
        for(unsigned offset=0;offset<ScheduledBytes;++offset)storage(display,RawBytes+offset)=scheduled[offset];
        for(unsigned offset=0;offset<SourceHashBytes;++offset)storage(display,RawBytes+ScheduledBytes+offset)=reinterpret_cast<const uint8_t *>(sourceHashes)[offset];
        stats.success=true;return stats;
    }
};
static_assert(CachedConverter::StoredBytes<=1194+396,"Cache exceeds post-pack color scratch");
}
