// SPDX-License-Identifier: MIT
#pragma once
#include <stdarg.h>
#include "VMRegistry.h"
#include "VMFail.h"

// The extension packages on the SD card, for the settings menu's Installed Extension
// page: a row per package, then a row for each reason one of them would not launch,
// then how the last launch went. Each row is one SendMsgPrintfln, which the C64 prints
// under the page's own header. It reads /VMS itself rather than VmRegistry's table,
// which is empty whenever the menu's source is not SD and keeps no ids, and it changes
// nothing the launch path reads.
//
// The page has rows 12-20 for it, 21 being the divider: nine rows. A row stays within
// 39 columns, because one that fills all 40 makes the screen editor advance by itself
// and the next message's leading return then leaves a blank row.
namespace VmPackageList {
enum : unsigned { Rows=9, Columns=39, ReservedNotes=3, IdColumns=10, ExtensionColumns=26 };
typedef char Row[Columns+1];

enum State : uint8_t { Listed, BadName, BadManifest };
// What launching would refuse, in the order tryLaunch() finds it.
enum Refusal : uint8_t { NoRefusal, ModuleFile, ClientFile, Services, CodeBase };
struct Entry {
    char id[sizeof(VmManifest::id)];  // the folder's name; a BadName one cut to fit and filtered
    char extension[sizeof(VmManifest::extension)];
    State state; Refusal refusal;
    uint32_t detail;                  // Services: the bits the host lacks; CodeBase: the module's base
};
struct Scan { Entry entry[VmRegistry::FolderLimit]; unsigned count; VmRegistry::Walk walk; bool cardRead; };

// A byte of a folder name goes to the screen only if an id could hold it. Anything
// else becomes '?', so the row still says which folder it is without handing the C64
// a control code.
static FLASHMEM char shown(char c){const char s[2]={c,0};return vm_path_component(s)?c:'?';}

// One folder of /VMS, `n` being getName's answer.
static FLASHMEM void entry(Scan &s,const VmHostId *host,const char *name,size_t n){
    Entry &e=s.entry[s.count++];
    memset(&e,0,sizeof e);
    for(size_t i=0;name[i]&&i+1<sizeof e.id;i++)e.id[i]=shown(name[i]);
    if(!VmRegistry::folderIsId(name,n)){e.state=BadName;return;}
    char root[80];snprintf(root,sizeof root,"/VMS/%s",name);VmManifest m{};
    if(!VmRegistry::readManifest(root,m)){e.state=BadManifest;return;}
    e.state=Listed;strcpy(e.extension,m.extension);
    VmImageHeader h{};
    const VmRegistry::Fault fault=VmRegistry::verify(root,m,h,VmRegistry::Quick);
    if(fault!=VmRegistry::Sound){e.refusal=fault==VmRegistry::ModuleBad?ModuleFile:ClientFile;return;}
    // What the host is owed, only when it can say: one without a descriptor is
    // launched without these checks, and one on another ABI refuses every package,
    // which layout() says once.
    if(!host||host->abi!=VM_ABI)return;
    if(const uint32_t missing=h.required_services&~host->services){e.refusal=Services;e.detail=missing;}
    else if(!vm_host_takes_code(h,host->code_floor)){e.refusal=CodeBase;e.detail=h.code_base;}
}

// `host` is the installed host's descriptor, null when there is no host or it has none.
// `card` is whether a card is in the slot and begun, which the caller finds out from the
// card itself (ShowVmPackages, DriveDirLoad.ino). It cannot be found here by reading:
// with the card pulled, SdFat still opens the root directory it remembers, so a missing
// card read as one without /VMS.
static FLASHMEM void collect(Scan &s,const VmHostId *host,bool card){
    s.count=0;s.cardRead=card;s.walk=VmRegistry::WalkNoDirectory;
    if(card)s.walk=VmRegistry::walk([&](const char *name,size_t n){entry(s,host,name,n);});
    // By id, ignoring case: the card lists folders in the order they were made.
    for(unsigned i=1;i<s.count;i++)
        for(unsigned j=i;j&&strcasecmp(s.entry[j-1].id,s.entry[j].id)>0;j--){const Entry t=s.entry[j];s.entry[j]=s.entry[j-1];s.entry[j-1]=t;}
}

struct Out { Row *row; unsigned count; unsigned wanted; };
// One row, if the block has room for it; `wanted` counts the ones that did not fit. A row
// too wide is cut at the last column with a '>', as MakeExtHostStr marks its own cut,
// rather than silently.
__attribute__((format(printf,2,3)))
static FLASHMEM void add(Out &out,const char *format,...){
    out.wanted++;
    if(out.count==Rows)return;
    char line[96];va_list args;va_start(args,format);vsnprintf(line,sizeof line,format,args);va_end(args);
    if(strlen(line)>Columns){line[Columns-1]='>';line[Columns]=0;}
    strcpy(out.row[out.count++],line);
}

// The k-th extension of a manifest's list; vm_manifest_extensions bounds each to 7.
static FLASHMEM bool extensionAt(const char *list,unsigned k,char (&ext)[8]){
    for(const char *p=list;*p;k--){
        const char *end=strchr(p,',');const size_t n=end?size_t(end-p):strlen(p);
        if(!k){const size_t c=n<sizeof ext-1?n:sizeof ext-1;memcpy(ext,p,c);ext[c]=0;return true;}
        if(!end)break;
        p=end+1;
    }
    return false;
}
static FLASHMEM bool claims(const Entry &e,const char *ext){return e.state==Listed&&VmRegistry::extensionMatches(e.extension,ext);}
// True when another package claims one of entry i's extensions, which tryLaunch then
// refuses for both.
static FLASHMEM bool shared(const Scan &s,unsigned i){
    char ext[8];
    for(unsigned k=0;extensionAt(s.entry[i].extension,k,ext);k++)
        for(unsigned j=0;j<s.count;j++)if(j!=i&&claims(s.entry[j],ext))return true;
    return false;
}

// x: not read at all; *: shares an extension; !: would not launch on this host.
static FLASHMEM void packageRow(Out &out,const Scan &s,unsigned i){
    const Entry &e=s.entry[i];
    const char mark=e.state!=Listed?'x':shared(s,i)?'*':e.refusal?'!':' ';
    char id[IdColumns+1];
    if(!e.id[0])strcpy(id,"?");
    else if(strlen(e.id)>IdColumns){memcpy(id,e.id,IdColumns-1);id[IdColumns-1]='>';id[IdColumns]=0;}
    else strcpy(id,e.id);
    const char *what=e.state==BadName?"(bad folder name)":e.state==BadManifest?"(no valid manifest.vmi)":e.extension;
    // A list too long is cut after the last whole extension that fits, with a '>'.
    char list[ExtensionColumns+1];
    if(strlen(what)<=ExtensionColumns)strcpy(list,what);
    else{size_t keep=0;for(size_t c=0;c<ExtensionColumns;c++)if(what[c]==',')keep=c;
        memcpy(list,what,keep);list[keep]='>';list[keep+1]=0;}
    add(out," %c%-*s %s",mark,(int)IdColumns,id,list);
}

// Fills `rows` and returns how many: the packages, then what would refuse a launch, then
// the last launch. When they do not all fit, the packages give way first, down to the
// rows ReservedNotes holds back, and a "+N more" row stands in for each part cut. `last`
// is the record the main image collected at boot (VmFail::captured), null when there
// was none.
static FLASHMEM unsigned layout(const Scan &s,bool installed,const VmHostId *host,const VmFailRecord *last,Row (&rows)[Rows]){
    Row reasonRows[Rows],lastRow[1];
    Out reasons{reasonRows,0,0},message{lastRow,0,0};
    unsigned listed=0;for(unsigned i=0;i<s.count;i++)listed+=s.entry[i].state==Listed;

    // The whole of /VMS. Over the limit or on a read error, find() refuses every launch.
    if(!s.cardRead)add(reasons," No SD card, or it can't be read.");
    else if(s.walk==VmRegistry::WalkNoDirectory)add(reasons," No /VMS folder on the SD card.");
    else if(s.walk==VmRegistry::WalkOverLimit)add(reasons," Over %u folders in /VMS: none launch.",(unsigned)VmRegistry::FolderLimit);
    else if(s.walk==VmRegistry::WalkError)add(reasons," /VMS read error: none launch.");
    else if(!s.count)add(reasons," No packages in /VMS.");

    // The host refuses every package or none of them on these two.
    if(listed){
        if(!installed)add(reasons," No host installed, so none launch.");
        else if(host&&host->abi!=VM_ABI)add(reasons," Host is ABI %lu, firmware needs %lu.",(unsigned long)host->abi,(unsigned long)VM_ABI);
    }

    // An extension more than one package claims, once, at the first of them.
    for(unsigned i=0;i<s.count;i++){
        char ext[8];
        for(unsigned k=0;s.entry[i].state==Listed&&extensionAt(s.entry[i].extension,k,ext);k++){
            bool earlier=false;unsigned others=0,other=0;
            for(unsigned j=0;j<s.count;j++)if(j!=i&&claims(s.entry[j],ext)){if(j<i)earlier=true;else if(!others++)other=j;}
            if(earlier||!others)continue;
            if(others==1)add(reasons," *.%s claimed by %s and %s",ext,s.entry[i].id,s.entry[other].id);
            else add(reasons," *.%s claimed by %u packages",ext,others+1);
        }
    }

    // What tryLaunch would say for a package of its own; the wording follows its messages.
    for(unsigned i=0;i<s.count;i++){
        const Entry &e=s.entry[i];
        if(e.state!=Listed)continue;
        switch(e.refusal){
            case ModuleFile:add(reasons," !%s: module file bad or missing",e.id);break;
            case ClientFile:add(reasons," !%s: client file bad or missing",e.id);break;
            case Services:add(reasons," !%s: host lacks service $%lx",e.id,(unsigned long)e.detail);break;
            case CodeBase:
                if(!host||!host->code_floor)add(reasons," !%s: host takes code at $%lx only",e.id,(unsigned long)VM_CODE_BASE);
                else add(reasons," !%s: code $%lx below host $%lx",e.id,(unsigned long)e.detail,
                         (unsigned long)vm_host_code_floor(host->code_floor));
                break;
            case NoRefusal:break;
        }
    }

    // Plain words for the three outcomes of a launch that started, VmFail::describe()'s
    // for the rest. The detail rides along as in the menu's own message (VmFail::report),
    // except an install's, which is its size.
    if(last){
        const uint8_t code=(uint8_t)last->code;
        const char *text=code==VmFail::Ok?"extension started":code==VmFail::Exited?"extension exited":
                         code==VmFail::Faulted?"extension crashed":VmFail::describe(code);
        if(last->detail&&code!=VmFail::Installed)add(message," Msg: %s ($%lx)",text,(unsigned long)last->detail);
        else add(message," Msg: %s",text);
    }

    const unsigned needed=reasons.wanted+message.count;
    const unsigned reserved=needed<ReservedNotes?needed:ReservedNotes;
    const unsigned room=Rows-reserved;
    const unsigned packages=s.count+needed<=Rows||s.count<=room?s.count:room-1;
    Out out{rows,0,0};
    for(unsigned i=0;i<packages;i++)packageRow(out,s,i);
    if(packages<s.count)add(out,"   +%u more",s.count-packages);

    const unsigned space=Rows-out.count;
    const unsigned reasonsShown=needed<=space?reasons.wanted:space-1-message.count;
    for(unsigned i=0;i<reasonsShown;i++)strcpy(out.row[out.count++],reasonRows[i]);
    if(reasonsShown<reasons.wanted)add(out,"   +%u more notes",reasons.wanted-reasonsShown);
    if(message.count)strcpy(out.row[out.count++],lastRow[0]);
    return out.count;
}

// rsShowExtPackages: every row, one message each, the C64 printing them down the page.
// A row the C64 did not read has cost SendMsgSerialStringBuf's three-second wait, so
// the rest are not sent after it.
static FLASHMEM void show(bool card,const VmHostId *host,bool installed,const VmFailRecord *last){
    Scan scan;collect(scan,host,card);
    Row rows[Rows];const unsigned count=layout(scan,installed,host,last,rows);
    for(unsigned i=0;i<count;i++)if(!SendMsgPrintfln("%s",rows[i]))break;
}
}
