// SPDX-License-Identifier: MIT
#pragma once
#include "VMABI.h"
namespace VmRegistry {
struct Manifest { char id[24],extension[8],module[32],client[32];uint32_t crc; };
struct Launch { uint32_t magic,manifest_crc;char root[80],content[256];uint32_t crc; };
static FLASHMEM bool component(const char *s) {
    if(!*s || strstr(s,".."))return false;
    for(;*s;s++)if(!((*s>='A'&&*s<='Z')||(*s>='a'&&*s<='z')||(*s>='0'&&*s<='9')||*s=='_'||*s=='-'||*s=='.'))return false;
    return true;
}
static FLASHMEM bool absolute(const char *s,size_t cap){
    if(s[0]!='/'||!memchr(s,0,cap)||strstr(s,"..")||strchr(s,'\\'))return false;
    return true;
}
// A bounded comma-separated extension list uses the existing manifest field;
// e.g. gb,gbc. This is generic routing, not a VM-specific firmware exception.
static FLASHMEM bool extensionMatches(const char *list,const char *ext){
    const size_t n=strlen(ext);for(const char *p=list;*p;){const char *end=strchr(p,',');size_t len=end?size_t(end-p):strlen(p);
        if(len==n&&!strncasecmp(p,ext,n))return true;if(!end)break;p=end+1;}return false;
}
static FLASHMEM bool validExtensions(const char *list){
    if(!*list||strlen(list)>7)return false;
    static const char protectedExtensions[][4]={"prg","crt","hex","p00","sid","kla","koa","ocp","pic","art","aas","hpi","txt","nfo","md","seq","d64","d71","d81","reu"};
    for(const char *p=list;*p;){char ext[8]{};const char *end=strchr(p,',');size_t n=end?size_t(end-p):strlen(p);
        if(!n||n>=sizeof ext)return false;memcpy(ext,p,n);if(!component(ext)||strchr(ext,'.'))return false;
        for(const auto &protectedExt:protectedExtensions)if(!strcasecmp(ext,protectedExt))return false;
        if(!end)return true;p=end+1;if(!*p)return false;}return false;
}
static FLASHMEM bool readManifest(const char *root,Manifest &m){
    char path[128],buf[192];if(!absolute(root,80)||snprintf(path,sizeof path,"%s/manifest.vmi",root)>=(int)sizeof path)return false;
    FsFile f=SD.sdfs.open(path,O_RDONLY);if(!f||f.isDirectory()||f.fileSize()>=sizeof buf){f.close();return false;}
    const uint32_t n=f.fileSize();const bool ok=f.read(buf,n)==(int)n;f.close();if(!ok)return false;buf[n]=0;
    char *line[6],*p=buf;unsigned count=0;
    while(*p&&count<6){line[count++]=p;while(*p&&*p!='\n'&&*p!='\r')p++;if(*p){*p++=0;while(*p=='\n'||*p=='\r')p++;}}
    if(count!=6||*p||strcmp(line[0],"VM1")||strcmp(line[5],"END"))return false;
    if(!component(line[1])||!validExtensions(line[2])||!component(line[3])||!component(line[4])||
       strlen(line[1])>=sizeof m.id||strlen(line[2])>=sizeof m.extension||strlen(line[3])>=sizeof m.module||strlen(line[4])>=sizeof m.client)return false;
    static const char protectedExtensions[][4]={"prg","crt","hex","p00","sid","kla","koa","ocp","pic","art","aas","hpi","txt","nfo","md","seq","d64","d71","d81","reu"};
    if(strchr(line[2],'.'))return false;
    for(const auto &ext:protectedExtensions)if(!strcasecmp(ext,line[2]))return false;
    const char *id=strrchr(root,'/');if(!id||strcmp(id+1,line[1]))return false;
    memset(&m,0,sizeof m);strcpy(m.id,line[1]);strcpy(m.extension,line[2]);strcpy(m.module,line[3]);strcpy(m.client,line[4]);
    m.crc=vm_crc32(&m,offsetof(Manifest,crc));return true;
}
// Registry limits are deliberate. Over-limit or ambiguous installs reject launch.
static FLASHMEM int find(const char *extension,const char *clientId,Launch &launch){
    FsFile directory=SD.sdfs.open("/VMS",O_RDONLY);if(!directory)return 0;
    unsigned scanned=0,found=0;FsFile item;
    while(item.openNext(&directory,O_RDONLY)){
        if(item.isDirectory()){
            char name[24],root[80];const size_t n=item.getName(name,sizeof name);
            if(++scanned>32){item.close();directory.close();return -1;}
            if(n&&n<sizeof name-1&&component(name)){
                snprintf(root,sizeof root,"/VMS/%s",name);Manifest m{};
                if(readManifest(root,m)&&((clientId&&strcmp(clientId,m.id)==0)||(!clientId&&extensionMatches(m.extension,extension)))){
                    found++;strcpy(launch.root,root);launch.manifest_crc=m.crc;
                }
            }
        }
        item.close();
    }
    const bool error=directory.getError();directory.close();return error||found>1?-1:(int)found;
}
static FLASHMEM bool consume(Launch &l){
    FsFile f=SD.sdfs.open("/VMS/launch.vml",O_RDONLY);
    const bool ok=f&&!f.isDirectory()&&f.fileSize()==sizeof l&&f.read(&l,sizeof l)==sizeof l;f.close();
    return ok&&l.magic==0x314c4d56&&l.crc==vm_crc32(&l,offsetof(Launch,crc))&&absolute(l.root,sizeof l.root)&&
        (!l.content[0]||absolute(l.content,sizeof l.content));
}
#ifndef MinimumBuild
static FLASHMEM bool preflight(const Launch &l){
    Manifest m{};if(!readManifest(l.root,m)||m.crc!=l.manifest_crc)return false;
    char path[128];snprintf(path,sizeof path,"%s/%s",l.root,m.module);FsFile f=SD.sdfs.open(path,O_RDONLY);VmImageHeader h{};
    if(!f||f.isDirectory()||f.fileSize()>UINT32_MAX||f.read(&h,64)!=64||!vm_valid_header(h,f.fileSize())){f.close();return false;}
    uint8_t block[512];uint32_t remain=vm_image_payload_bytes(h),c=~0u;
    while(remain){const unsigned n=remain>sizeof block?sizeof block:remain;if(f.read(block,n)!=(int)n){f.close();return false;}
        for(unsigned i=0;i<n;i++){c^=block[i];for(unsigned b=0;b<8;b++)c=(c>>1)^((0u-(c&1))&0xedb88320u);}remain-=n;}
    f.close();if(~c!=h.payload_crc)return false;
    snprintf(path,sizeof path,"%s/%s",l.root,m.client);f=SD.sdfs.open(path,O_RDONLY);uint8_t d[128];
    if(!f||f.fileSize()!=0x6070||!f.seekSet(0x4070)||f.read(d,128)!=128||memcmp(d,"VMH1",4)||d[4]!=VM_ABI||
       !memchr(d+16,0,24)||strcmp((char *)d+16,m.id)||vm_crc32(d,124)!=*(uint32_t *)(d+124)){f.close();return false;}
    c=~0u;for(unsigned bank=0;bank<2;bank++){
        if(!f.seekSet(64+16+bank*8208)){f.close();return false;}
        for(unsigned offset=0;offset<8192;offset+=512){if(f.read(block,512)!=512){f.close();return false;}
            for(auto v:block){c^=v;for(unsigned b=0;b<8;b++)c=(c>>1)^((0u-(c&1))&0xedb88320u);}}
    }f.close();return ~c==*(uint32_t *)(d+8);
}
static char extensions[32][8];static uint8_t extensionCount;
static FLASHMEM void refresh(bool sd){
    extensionCount=0;if(!sd)return;FsFile dir=SD.sdfs.open("/VMS",O_RDONLY);if(!dir)return;
    FsFile item;unsigned scanned=0;
    while(item.openNext(&dir,O_RDONLY)){
        if(item.isDirectory()){
            if(++scanned>32){extensionCount=0;item.close();break;}
            char id[24],root[80];auto n=item.getName(id,sizeof id);Manifest m{};
            if(n&&n<sizeof id-1&&component(id)){
                snprintf(root,sizeof root,"/VMS/%s",id);
                if(readManifest(root,m))strcpy(extensions[extensionCount++],m.extension);
            }
        }item.close();
    }if(dir.getError())extensionCount=0;dir.close();
}
static FLASHMEM bool associated(const char *name){
    const char *ext=strrchr(name,'.');if(!ext)return false;
    for(unsigned i=0;i<extensionCount;i++)if(extensionMatches(extensions[i],ext+1))return true;return false;
}
static FLASHMEM bool tryLaunch(uint8_t source,const char *directory,const char *name){
    if(source!=rmtSD)return false;
    const char *ext=strrchr(name,'.');if(!ext)return false;ext++;
    Launch l{};char selected[256];
    if(snprintf(selected,sizeof selected,"%s%s%s",directory,directory[strlen(directory)-1]=='/'?"":"/",name)>=(int)sizeof selected){SendMsgPrintfln("VM path too long");return true;}
    char id[24]{};const char *clientId=nullptr;
    if(!strcasecmp(ext,"crt")){
        // Generic descriptor occupies the third CHIP, after the 16 KiB boot bank.
        uint8_t d[128];FsFile f=SD.sdfs.open(selected,O_RDONLY);
        const bool ok=f&&f.seekSet(0x4070)&&f.read(d,sizeof d)==sizeof d;f.close();
        if(!ok||memcmp(d,"VMH1",4))return false;
        if(d[4]!=VM_ABI||!memchr(d+16,0,24)||vm_crc32(d,124)!=*(uint32_t *)(d+124)){SendMsgPrintfln("Invalid VM client");return true;}
        strcpy(id,(char *)d+16);clientId=id;
    }
    const int found=find(ext,clientId,l);
    if(!found){if(!clientId)return false;SendMsgPrintfln("VM package missing in /VMS");return true;}
    if(found<0){SendMsgPrintfln("Ambiguous or over-limit VM registry");return true;}
    if(!clientId)strcpy(l.content,selected);
    if(!preflight(l)){SendMsgPrintfln("VM package/client failed validation");return true;}
    l.magic=0x314c4d56;l.crc=vm_crc32(&l,offsetof(Launch,crc));
    FsFile f=SD.sdfs.open("/VMS/launch.vml",O_WRONLY|O_CREAT|O_TRUNC);
    const bool saved=f&&f.write(&l,sizeof l)==sizeof l&&f.sync();f.close();
    Launch check{};if(!saved||!consume(check)||memcmp(&check,&l,sizeof l)){SendMsgPrintfln("VM launch record write failed");return true;}
    // The EEPROM flag is the one-shot commit, and is cleared by MinimalBoot.
    EEPwriteStr(eepAdCrtBootName,"@VM1");EEPROM.write(eepAdMinBootInd,MinBootInd_ExecuteMin);
    SetResetAssert;delay(20);REBOOT;return true;
}
#endif
}
