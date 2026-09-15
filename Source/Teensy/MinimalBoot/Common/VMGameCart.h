// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#ifndef FLASHMEM
#define FLASHMEM
#endif

// MGC1 cartridge reader. No heap, SD library, platform globals, or C64 work.
// Byte offsets are independent of the C64 cartridge's bank decoder.
namespace VmGameCart {
namespace {
static constexpr uint32_t DescriptorOffset=0x4070,PrefixBytes=0x6070,BlockBytes=4096,
  MaxEntries=32,MaxFileBytes=64u*1024u*1024u;
struct Entry {char name[64];uint32_t flags,bytes,offset,storedBytes,crc,blockCount,blockTable,blockCrc;};
struct Metadata {uint32_t totalBytes,count,moduleIndex,contentIndex,requiredServices;
  char id[24],title[64],content[64],minimumFirmware[24];};
inline uint16_t u16(const uint8_t*p){return uint16_t(p[0])|uint16_t(p[1])<<8;}
inline uint32_t u32(const uint8_t*p){return uint32_t(u16(p))|uint32_t(u16(p+2))<<16;}
static FLASHMEM uint32_t crcContinue(uint32_t crc,const uint8_t*p,uint32_t n){while(n--){crc^=*p++;for(unsigned b=0;b<8;b++)crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);}return crc;}
inline uint32_t crc32(const void*p,uint32_t n){return ~crcContinue(~0u,static_cast<const uint8_t*>(p),n);}
inline char upper(char c){return c>='a'&&c<='z'?char(c-'a'+'A'):c;}
// .MPE is the user-facing name for an MGC1 container; its internal CRT prefix
// remains unchanged. The suffix never replaces structural/CRC validation.
static FLASHMEM bool isMpeFile(const char *path){
  if(!path)return false;const char *ext=strrchr(path,'.');
  return ext&&upper(ext[1])=='M'&&upper(ext[2])=='P'&&upper(ext[3])=='E'&&!ext[4];
}
inline int compare(const char*a,const char*b){while(*a&&upper(*a)==upper(*b)){++a;++b;}return int(uint8_t(upper(*a)))-int(uint8_t(upper(*b)));}
inline bool bounded(uint32_t at,uint32_t n,uint32_t total){return at<=total&&n<=total-at;}
static FLASHMEM bool validName(const char*s){
  unsigned n=0;bool initial=true;
  for(;*s;++s){if(++n>=64)return false;const char c=*s;
    if(c=='/'){if(initial)return false;initial=true;continue;}
    const bool alpha=(c>='A'&&c<='Z')||(c>='a'&&c<='z'),digit=c>='0'&&c<='9';
    if(initial&&!alpha&&!digit)return false;
    if(!alpha&&!digit&&c!='_'&&c!='-'&&c!='.')return false;
    if(c=='.'&&s[1]=='.')return false;initial=false;
  }return n&&!initial;
}
static FLASHMEM bool text(const uint8_t*p,uint32_t n,char*out){
  bool ended=false;for(uint32_t i=0;i<n;++i){const uint8_t c=p[i];if(!c)ended=true;else if(ended||c<32||c>126)return false;out[i]=char(c);}return ended;
}
static FLASHMEM bool zero(const uint8_t*p,uint32_t n){while(n--)if(*p++)return false;return true;}

// Safe independent LZ4 block decoder; output never exceeds the 4 KiB cache.
// Implements lz4/lz4 doc/lz4_Block_format.md; no imported implementation code.
static FLASHMEM bool decodeLz4(const uint8_t*in,uint32_t inputBytes,uint8_t*out,uint32_t outputBytes){
  if(!outputBytes||outputBytes>BlockBytes)return false;uint32_t ip=0,op=0;
  while(ip<inputBytes){const uint8_t token=in[ip++];uint32_t literals=token>>4;
    if(literals==15){uint8_t b;do{if(ip==inputBytes)return false;b=in[ip++];literals+=b;if(literals>outputBytes)return false;}while(b==255);}
    if(!bounded(ip,literals,inputBytes)||!bounded(op,literals,outputBytes))return false;
    memcpy(out+op,in+ip,literals);ip+=literals;op+=literals;
    if(ip==inputBytes)return op==outputBytes;
    if(!bounded(ip,2,inputBytes))return false;const uint32_t distance=u16(in+ip);ip+=2;
    if(!distance||distance>op)return false;uint32_t match=token&15;
    if(match==15){uint8_t b;do{if(ip==inputBytes)return false;b=in[ip++];match+=b;if(match>outputBytes)return false;}while(b==255);}
    match+=4;if(!bounded(op,match,outputBytes))return false;
    while(match--){out[op]=out[op-distance];++op;}
  }return false;
}

class Reader {
 public:
  using ReadAt=bool(*)(void*,uint32_t,void*,uint32_t);
  const Metadata& meta()const{return meta_;}
  void invalidate(){ready_=false;cacheEntry_=cacheBlock_=0xffffffffu;entryIds_[0]=entryIds_[1]=0xffffffffu;}
  bool FLASHMEM open(void*context,ReadAt readAt,uint32_t fileBytes){
    invalidate();context_=context;readAt_=readAt;fileBytes_=fileBytes;memset(&meta_,0,sizeof meta_);
    if(!readAt||fileBytes<PrefixBytes||fileBytes>MaxFileBytes)return false;
    uint8_t h[256];if(!physical(0,h,64)||memcmp(h,"C64 CARTRIDGE   ",16)||h[16]||h[17]||h[18]||h[19]!=64||h[22]||h[23]!=32)return false;
    for(unsigned i=0;i<3;++i){if(!physical(64+i*8208,h,16)||memcmp(h,"CHIP",4)||h[4]||h[5]||h[6]!=0x20||h[7]!=0x10||h[10]||h[11]!=(i==2?1:0)||h[12]!=(i==1?0xa0:0x80)||h[13]||h[14]!=0x20||h[15])return false;}
    if(!physical(DescriptorOffset,h,256)||memcmp(h,"MGC1",4)||u16(h+4)!=1||u16(h+6)!=256)return false;
    const uint32_t expected=u32(h+56);memset(h+56,0,4);if(crc32(h,256)!=expected)return false;
    meta_.totalBytes=u32(h+8);meta_.count=u32(h+20);meta_.moduleIndex=u32(h+40);meta_.contentIndex=u32(h+44);meta_.requiredServices=u32(h+48);
    if(meta_.totalBytes!=fileBytes||u32(h+12)!=1||u32(h+16)!=BlockBytes||meta_.count<2||meta_.count>MaxEntries||u32(h+24)!=PrefixBytes||u32(h+28)!=128||!zero(h+52,4)||!zero(h+60,4)||!zero(h+240,16))return false;
    if(!text(h+64,24,meta_.id)||!text(h+88,64,meta_.title)||!text(h+152,64,meta_.content)||!text(h+216,24,meta_.minimumFirmware)||!validName(meta_.content))return false;
    if(meta_.id[0]<'A'||meta_.id[0]>'Z')return false;
    for(const char*p=meta_.id;*p;++p)if(!((*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='_'||*p=='-'))return false;
    uint32_t boot=~0u;for(unsigned half=0;half<2;++half)for(unsigned p=0;p<8192;p+=256){if(!physical(80+half*8208+p,packed_,256))return false;boot=crcContinue(boot,packed_,256);}
    if(~boot!=u32(h+36)||!checkCrc(PrefixBytes,meta_.count*128,u32(h+32)))return false;
    uint32_t cursor=PrefixBytes+meta_.count*128;Entry previous{},e{};
    for(uint32_t i=0;i<meta_.count;++i){if(!loadEntry(i,e)||!e.bytes||e.bytes>MaxFileBytes||(i&&compare(previous.name,e.name)>=0))return false;
      for(uint32_t j=0;j<i;++j){Entry prior{};if(!loadEntry(j,prior))return false;const size_t n=strlen(prior.name);bool prefix=true;for(size_t k=0;k<n;++k)if(upper(prior.name[k])!=upper(e.name[k])){prefix=false;break;}if(prefix&&e.name[n]=='/')return false;}
      if(!e.flags){if(e.blockCount||e.blockTable||e.blockCrc||e.offset!=cursor||e.storedBytes!=e.bytes||!bounded(cursor,e.bytes,fileBytes))return false;cursor+=e.bytes;}
      else{
        const uint32_t blocks=(e.bytes+BlockBytes-1)/BlockBytes;
        if(e.flags!=1||e.blockCount!=blocks||e.blockTable!=cursor||!bounded(cursor,blocks*16,fileBytes)||!checkCrc(cursor,blocks*16,e.blockCrc))return false;
        cursor+=blocks*16;if(e.offset!=cursor)return false;
        for(uint32_t j=0;j<blocks;++j){uint8_t b[16];if(!physical(e.blockTable+j*16,b,16))return false;
          const uint32_t length=u32(b+4)&0x7fffffffu,raw=u32(b+8),remain=e.bytes-j*BlockBytes;
          if(u32(b)!=cursor||raw!=(remain>BlockBytes?BlockBytes:remain)||!length||length>raw||((u32(b+4)&0x80000000u)&&length!=raw)||!bounded(cursor,length,fileBytes))return false;cursor+=length;
        }if(cursor-e.offset!=e.storedBytes)return false;
      }previous=e;
    }
    if(cursor!=fileBytes||meta_.moduleIndex>=meta_.count||meta_.contentIndex>=meta_.count||meta_.moduleIndex==meta_.contentIndex)return false;
    if(!loadEntry(meta_.moduleIndex,e)||strcmp(e.name,"engine.mvm")||e.flags||!loadEntry(meta_.contentIndex,e)||strcmp(e.name,meta_.content))return false;
    ready_=true;return true;
  }
  bool FLASHMEM entry(uint32_t index,Entry&out){return ready_&&loadEntry(index,out);}
  int FLASHMEM find(const char*name){if(!ready_||!name||!validName(name))return -1;Entry e{};for(uint32_t i=0;i<meta_.count;++i){if(!loadEntry(i,e))return -1;const int c=compare(e.name,name);if(!c)return int(i);if(c>0)break;}return -1;}
  bool FLASHMEM readEntry(uint32_t index,uint32_t at,void*dest,uint32_t count){
    if(!ready_||(!dest&&count))return false;Entry e{};if(!loadEntry(index,e)||!bounded(at,count,e.bytes))return false;if(!count)return true;
    if(!e.flags)return physical(e.offset+at,dest,count);
    auto*out=static_cast<uint8_t*>(dest);
    while(count){const uint32_t block=at/BlockBytes,within=at%BlockBytes;
      if(cacheEntry_!=index||cacheBlock_!=block){cacheEntry_=cacheBlock_=0xffffffffu;uint8_t b[16];if(!physical(e.blockTable+block*16,b,16))return false;
        const uint32_t off=u32(b),stored=u32(b+4),length=stored&0x7fffffffu,raw=u32(b+8),remain=e.bytes-block*BlockBytes;
        if(!length||raw!=(remain>BlockBytes?BlockBytes:remain)||length>raw||!bounded(off,length,fileBytes_)||off<e.offset||!bounded(off-e.offset,length,e.storedBytes))return false;
        if(stored&0x80000000u){if(length!=raw||!physical(off,cache_,raw))return false;}
        else if(!physical(off,packed_,length)||!decodeLz4(packed_,length,cache_,raw))return false;
        if(crc32(cache_,raw)!=u32(b+12))return false;cacheEntry_=index;cacheBlock_=block;
      }
      const uint32_t take=count<BlockBytes-within?count:BlockBytes-within;memcpy(out,cache_+within,take);at+=take;out+=take;count-=take;
    }return true;
  }
  bool FLASHMEM verifyEntry(uint32_t index){
    Entry e{};if(!entry(index,e))return false;uint8_t bytes[256];uint32_t crc=~0u;
    for(uint32_t at=0;at<e.bytes;){const uint32_t n=e.bytes-at>sizeof bytes?sizeof bytes:e.bytes-at;if(!readEntry(index,at,bytes,n))return false;crc=crcContinue(crc,bytes,n);at+=n;}return ~crc==e.crc;
  }
 private:
  Metadata meta_{};void*context_=nullptr;ReadAt readAt_=nullptr;uint32_t fileBytes_=0;bool ready_=false;
  uint32_t cacheEntry_=0xffffffffu,cacheBlock_=0xffffffffu,entryIds_[2]={0xffffffffu,0xffffffffu},entryNext_=0;
  Entry entries_[2]{};uint8_t cache_[BlockBytes]{},packed_[BlockBytes]{};
  bool FLASHMEM physical(uint32_t at,void*out,uint32_t n){return bounded(at,n,fileBytes_)&&readAt_(context_,at,out,n);}
  bool FLASHMEM checkCrc(uint32_t at,uint32_t n,uint32_t expected){uint32_t crc=~0u;while(n){const uint32_t take=n>256?256:n;if(!physical(at,packed_,take))return false;crc=crcContinue(crc,packed_,take);at+=take;n-=take;}return ~crc==expected;}
  bool FLASHMEM loadEntry(uint32_t index,Entry&out){
    if(index>=meta_.count)return false;for(unsigned i=0;i<2;++i)if(entryIds_[i]==index){out=entries_[i];return true;}
    uint8_t b[128];if(!physical(PrefixBytes+index*128,b,128)||!text(b,64,out.name)||!validName(out.name)||!zero(b+96,32))return false;
    if(upper(out.name[0])=='S'&&upper(out.name[1])=='A'&&upper(out.name[2])=='V'&&upper(out.name[3])=='E'&&upper(out.name[4])=='S'&&(!out.name[5]||out.name[5]=='/'))return false;
    out.flags=u32(b+64);out.bytes=u32(b+68);out.offset=u32(b+72);out.storedBytes=u32(b+76);out.crc=u32(b+80);out.blockCount=u32(b+84);out.blockTable=u32(b+88);out.blockCrc=u32(b+92);
    const unsigned slot=entryNext_++&1;entryIds_[slot]=index;entries_[slot]=out;return true;
  }
};
}
}
