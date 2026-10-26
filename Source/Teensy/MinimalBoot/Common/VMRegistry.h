// SPDX-License-Identifier: MIT
#pragma once
#include "VMABI.h"
#include "VMBootImage.h"
#include "VMHostABI.h"
namespace VmRegistry {
// The record, the manifest and the path rules are the published host contract
// (VMHostABI.h): the other side of the reboot may be a third-party host.
using Manifest = VmManifest;
using Launch = VmLaunchRecord;
static FLASHMEM bool component(const char *s){return vm_path_component(s);}
static FLASHMEM bool absolute(const char *s,size_t cap){return vm_path_absolute(s,cap);}
// A bounded comma-separated extension list uses the existing manifest field;
// e.g. gb,gbc. This is generic routing, not a VM-specific firmware exception.
static FLASHMEM bool extensionMatches(const char *list,const char *ext){
    const size_t n=strlen(ext);for(const char *p=list;*p;){const char *end=strchr(p,',');size_t len=end?size_t(end-p):strlen(p);
        if(len==n&&!strncasecmp(p,ext,n))return true;if(!end)break;p=end+1;}return false;
}
static FLASHMEM bool validExtensions(const char *list){return vm_manifest_extensions(list);}
static FLASHMEM bool readManifest(const char *root,Manifest &m){
    char path[128],buf[192];if(!absolute(root,80)||snprintf(path,sizeof path,"%s/manifest.vmi",root)>=(int)sizeof path)return false;
    FsFile f=SD.sdfs.open(path,O_RDONLY);if(!f||f.isDirectory()||f.fileSize()>=sizeof buf){f.close();return false;}
    const uint32_t n=f.fileSize();const bool ok=f.read(buf,n)==(int)n;f.close();if(!ok||memchr(buf,0,n))return false;buf[n]=0;
    return vm_manifest_parse(buf,root,m);
}
// Registry limits are deliberate. Over-limit or ambiguous installs reject launch.
enum : unsigned { FolderLimit=32 };
enum Walk : uint8_t { WalkNoDirectory, WalkDone, WalkOverLimit, WalkError };
// A folder of /VMS can hold a package only if its name is an id: 1-23 characters of
// component()'s set. `n` is getName's answer.
static FLASHMEM bool folderIsId(const char *name,size_t n){return n&&n<sizeof(Manifest::id)&&component(name);}
// Calls visit(context,name,n) for each folder of /VMS, the name wide enough that a folder
// that is no id can still be shown. Every reader of /VMS goes through here, so they agree
// on where the walk stops. A function pointer, not a template: FLASHMEM on a template
// does not reach every instance, and refresh()'s landed in ITCM.
using Visit = void (*)(void *context,const char *name,size_t n);
static FLASHMEM Walk walk(Visit visit,void *context){
    FsFile directory=SD.sdfs.open("/VMS",O_RDONLY);if(!directory)return WalkNoDirectory;
    unsigned scanned=0;FsFile item;
    while(item.openNext(&directory,O_RDONLY)){
        if(item.isDirectory()){
            if(++scanned>FolderLimit){item.close();directory.close();return WalkOverLimit;}
            char name[64]{};const size_t n=item.getName(name,sizeof name);name[sizeof name-1]=0;
            visit(context,name,n);
        }
        item.close();
    }
    const bool error=directory.getError();directory.close();return error?WalkError:WalkDone;
}
// find() returns how many packages matched (0 or 1), or one of these. On
// Ambiguous, `rival` (if given) holds the first claimant's id and how many claim
// it; launch.root is the last claimant's.
enum : int { OverLimitOrError=-1, Ambiguous=-2 };
struct Rival { char id[sizeof(Manifest::id)]; unsigned claims; };
struct Search { const char *extension; const char *clientId; Launch &launch; Rival *rival; unsigned found; };
static FLASHMEM void claimant(void *context,const char *name,size_t n){
    Search &s=*(Search *)context;if(!folderIsId(name,n))return;
    char root[80];snprintf(root,sizeof root,"/VMS/%s",name);Manifest m{};
    if(readManifest(root,m)&&((s.clientId&&strcmp(s.clientId,m.id)==0)||(!s.clientId&&extensionMatches(m.extension,s.extension)))){
        if(s.found++==1&&s.rival)strcpy(s.rival->id,strrchr(s.launch.root,'/')+1);strcpy(s.launch.root,root);s.launch.manifest_crc=m.crc;
    }
}
static FLASHMEM int find(const char *extension,const char *clientId,Launch &launch,Rival *rival=nullptr){
    Search search{extension,clientId,launch,rival,0};
    const Walk w=walk(claimant,&search);const unsigned found=search.found;
    if(w==WalkNoDirectory)return 0;
    if(w==WalkOverLimit||w==WalkError)return OverLimitOrError;
    if(rival)rival->claims=found;
    return found>1?Ambiguous:(int)found;
}
static FLASHMEM bool consume(Launch &l){
    FsFile f=SD.sdfs.open("/VMS/launch.vml",O_RDONLY);
    const bool ok=f&&!f.isDirectory()&&f.fileSize()==sizeof l&&f.read(&l,sizeof l)==sizeof l;f.close();
    return ok&&vm_launch_valid(l);
}
#ifndef MinimumBuild
enum Fault : uint8_t { Sound, ModuleBad, ClientBad };
// Quick leaves out the payload and client bank CRCs, which read every byte of both files.
enum Depth : uint8_t { Quick, Full };
// Whether the package at `root` has a module image and a client cartridge that launch
// would accept. `h` is the module's header once it reads.
static FLASHMEM Fault verify(const char *root,const Manifest &m,VmImageHeader &h,Depth depth=Full){
    char path[128];snprintf(path,sizeof path,"%s/%s",root,m.module);FsFile f=SD.sdfs.open(path,O_RDONLY);
    if(!f||f.isDirectory()||f.fileSize()>UINT32_MAX||f.read(&h,64)!=64||!vm_valid_header(h,f.fileSize())){f.close();return ModuleBad;}
    uint8_t block[512];uint32_t remain=depth==Full?vm_image_payload_bytes(h):0,c=~0u;
    while(remain){const unsigned n=remain>sizeof block?sizeof block:remain;if(f.read(block,n)!=(int)n){f.close();return ModuleBad;}
        for(unsigned i=0;i<n;i++){c^=block[i];for(unsigned b=0;b<8;b++)c=(c>>1)^((0u-(c&1))&0xedb88320u);}remain-=n;}
    f.close();if(depth==Full&&~c!=h.payload_crc)return ModuleBad;
    snprintf(path,sizeof path,"%s/%s",root,m.client);f=SD.sdfs.open(path,O_RDONLY);uint8_t d[128];
    if(!f||f.fileSize()!=0x6070||!f.seekSet(0x4070)||f.read(d,128)!=128||memcmp(d,"VMH1",4)||d[4]!=VM_ABI||
       !memchr(d+16,0,24)||strcmp((char *)d+16,m.id)||vm_crc32(d,124)!=*(uint32_t *)(d+124)){f.close();return ClientBad;}
    if(depth==Quick){f.close();return Sound;}
    c=~0u;for(unsigned bank=0;bank<2;bank++){
        if(!f.seekSet(64+16+bank*8208)){f.close();return ClientBad;}
        for(unsigned offset=0;offset<8192;offset+=512){if(f.read(block,512)!=512){f.close();return ClientBad;}
            for(auto v:block){c^=v;for(unsigned b=0;b<8;b++)c=(c>>1)^((0u-(c&1))&0xedb88320u);}}
    }f.close();return ~c==*(uint32_t *)(d+8)?Sound:ClientBad;
}
static FLASHMEM bool preflight(const Launch &l,VmImageHeader *out=nullptr){
    Manifest m{};if(!readManifest(l.root,m)||m.crc!=l.manifest_crc)return false;
    VmImageHeader h{};const Fault fault=verify(l.root,m,h);
    if(out)*out=h;
    return fault==Sound;
}
// The extensions the installed packages claim, so launching an ordinary file
// need not rescan /VMS. Rebuilt wherever the SD listing is, which keeps the
// table no staler than the menu the launch was chosen from.
//
// Unknown is not NotAssociated. Only find() can report what it refuses; a table
// that answered "no" after an overflow, a read error or no scan at all would
// send an extension package into stock file handling with nothing said. So
// every case where the table cannot speak for /VMS answers Unknown, and the
// caller falls back to the scan.
enum Association : uint8_t { NotAssociated, Associated, Unknown };
static char extensions[32][sizeof(Manifest::extension)];static uint8_t extensionCount;static bool extensionsKnown;
static FLASHMEM void recordExtension(void *,const char *name,size_t n){
    if(!folderIsId(name,n))return;
    char root[80];Manifest m{};snprintf(root,sizeof root,"/VMS/%s",name);
    if(readManifest(root,m))strcpy(extensions[extensionCount++],m.extension);
}
static FLASHMEM void refresh(bool sd){
    extensionCount=0;extensionsKnown=false;if(!sd)return;
    const Walk w=walk(recordExtension,nullptr);
    extensionsKnown=w==WalkDone;if(!extensionsKnown)extensionCount=0;
}
static FLASHMEM Association associated(const char *name){
    if(!extensionsKnown)return Unknown;
    const char *ext=strrchr(name,'.');if(!ext)return NotAssociated;
    for(unsigned i=0;i<extensionCount;i++)if(extensionMatches(extensions[i],ext+1))return Associated;
    return NotAssociated;
}
// A packaged .crt carries the generic client descriptor in its third CHIP, after the 16 KiB boot bank.
// True when d[] holds one; it is not validated past the magic.
static FLASHMEM bool clientDescriptor(const char *path,uint8_t d[128]){
    FsFile f=SD.sdfs.open(path,O_RDONLY);
    const bool ok=f&&f.seekSet(0x4070)&&f.read(d,128)==128;f.close();
    return ok&&!memcmp(d,"VMH1",4);
}
static FLASHMEM bool tryLaunch(uint8_t source,const char *directory,const char *name){
    if(source!=rmtSD)return false;
    const char *ext=strrchr(name,'.');if(!ext)return false;ext++;
    Launch l{};char selected[256];
    if(snprintf(selected,sizeof selected,"%s%s%s",directory,directory[strlen(directory)-1]=='/'?"":"/",name)>=(int)sizeof selected){SendMsgPrintfln("VM path too long");return true;}
    // The host gets one slash between components. The browser's path is "/" plus "/<dir>"
    // per level, so below the root it starts "//", and stored or remote paths can carry more.
    {char *w=selected;for(const char *r=selected;*r;r++)if(*r!='/'||w==selected||w[-1]!='/')*w++=*r;*w=0;}
    char id[24]{};const char *clientId=nullptr;
    if(!strcasecmp(ext,"crt")){
        uint8_t d[128];if(!clientDescriptor(selected,d))return false;
        if(d[4]!=VM_ABI||!memchr(d+16,0,24)||vm_crc32(d,124)!=*(uint32_t *)(d+124)){SendMsgPrintfln("Invalid VM client");return true;}
        strcpy(id,(char *)d+16);clientId=id;
    }
    Rival rival{};const int found=find(ext,clientId,l,&rival);
    if(!found){if(!clientId)return false;SendMsgPrintfln("VM package missing in /VMS");return true;}
    if(found==Ambiguous){
        if(rival.claims==2)SendMsgPrintfln(".%s is claimed by both\r\n%s\r\n%s",ext,rival.id,strrchr(l.root,'/')+1);
        else SendMsgPrintfln(".%s is claimed by %u packages\r\n%s\r\n%s\r\nand %u more",ext,rival.claims,rival.id,strrchr(l.root,'/')+1,rival.claims-2);
        return true;}
    if(found<0){SendMsgPrintfln("Over-limit or unreadable /VMS");return true;}
    if(!clientId)strcpy(l.content,selected);
    VmImageHeader image{};
    if(!preflight(l,&image)){SendMsgPrintfln("VM package/client failed validation");return true;}
    if(!VmBootImage::installed()){SendMsgPrintfln("No extension host installed");return true;}
    VmHostId hostId{};
    if(VmBootImage::identity(hostId)){
        // Through displayName, like every other message that shows the name. These reach
        // the same C64 screen, and reading the field in place put its raw bytes there:
        // the width tracked the field but nothing stopped a control code, so a refusal
        // could clear the screen it was printing on.
        char hostName[VmBootImage::nameBytes];
        VmBootImage::displayName(hostName,sizeof hostName,&hostId);
        if(hostId.abi!=VM_ABI){SendMsgPrintfln("%s host is ABI %lu, not %lu",hostName,
                        (unsigned long)hostId.abi,(unsigned long)VM_ABI);return true;}
        if(image.required_services&~hostId.services){SendMsgPrintfln("%s host lacks service $%lx",hostName,
                        (unsigned long)(image.required_services&~hostId.services));return true;}
        // Says the number rather than a capability, because that is what the module
        // asked for and what a rebuild would have to change. A host that states no
        // floor is told apart, since it is most often one installed before the
        // field rather than one whose code fills the window.
        if(!vm_host_takes_code(image,hostId.code_floor)){
            if(!hostId.code_floor)SendMsgPrintfln("%s host states no code floor, so takes $%lx only",hostName,
                        (unsigned long)VM_CODE_BASE);
            else SendMsgPrintfln("%s host needs code at $%lx, not $%lx",hostName,
                        (unsigned long)vm_host_code_floor(hostId.code_floor),(unsigned long)image.code_base);
            return true;}}
    l.magic=VM_LAUNCH_MAGIC;l.crc=vm_crc32(&l,offsetof(Launch,crc));
    FsFile f=SD.sdfs.open("/VMS/launch.vml",O_WRONLY|O_CREAT|O_TRUNC);
    const bool saved=f&&f.write(&l,sizeof l)==sizeof l&&f.sync();f.close();
    Launch check{};if(!saved||!consume(check)||memcmp(&check,&l,sizeof l)){SendMsgPrintfln("VM launch record write failed");return true;}
    // The EEPROM flag is the one-shot commit, and is cleared by MinimalBoot.
    EEPwriteStr(eepAdCrtBootName,VM_HOST_MARKER);
    // C64/C128 is only known once the menu has reported. A launch made before that is sent through the
    // menu first (RemoteLaunch), so the pair is only left invalid, rather than hand over a guess, if not.
    const uint8_t vid=IO1[wRegVid_TOD_Clks]&7,machine[2]={vid,uint8_t((VidTODClksKnown&rvtcC128)?~vid:vid)};
    EEPwriteNBuf(VM_EEP_MACHINE_ADDR,machine,2);EEPROM.write(eepAdMinBootInd,MinBootInd_ExecuteMin);
    RebootTR();return true;
}
#endif
}
