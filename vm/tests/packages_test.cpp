// SPDX-License-Identifier: MIT
// The Installed Extension page's package list (VMPackageList.h) over real packages
// written by tools/lib/extension.mjs: the rows, their marks and cuts, what would refuse
// a launch, the last launch, and the nine-row budget the page has for all of it.
#include "fake_sd.h"
static void arm_dcache_flush_delete(void *,size_t){}
static void arm_dcache_delete(void *,size_t){}
static struct { void printf(const char *,...){} } Serial;
#include "../../Source/Teensy/MinimalBoot/Common/VMPackageList.h"
#include <initializer_list>
#include <string>

using namespace VmPackageList;
typedef std::vector<std::string> Lines;

static void put(const fs::path &p,const std::string &s){fs::create_directories(p.parent_path());std::ofstream(p,std::ios::binary)<<s;}
static void manifest(const std::string &id,const std::string &list){
    put(base/"VMS"/id/"manifest.vmi","VM1\n"+id+"\n"+list+"\nengine.mvm\nclient.crt\nEND\n");}

// Every row the page would print. Each must fit the 39 columns and hand the C64 nothing
// but printable ASCII -- no control code, and no quote to put the editor in quote mode.
static Lines rows(const VmHostId *host,bool installed=true,const VmFailRecord *last=nullptr,bool card=true){
    Scan s;collect(s,host,card);Row r[Rows];const unsigned n=layout(s,installed,host,last,r);
    assert(n>=1&&n<=Rows);
    Lines out(r,r+n);
    for(const auto &row:out){assert(row.size()<=Columns);for(unsigned char c:row)assert(c>=0x20&&c<0x7f&&c!='"');}
    return out;
}
static void expect(const Lines &got,std::initializer_list<const char *> want){
    bool same=got.size()==want.size();
    if(same){size_t i=0;for(const char *w:want)same&=got[i++]==w;}
    if(same)return;
    fputs("rows:\n",stderr);for(const auto &g:got)fprintf(stderr,"  [%s]\n",g.c_str());
    fputs("expected:\n",stderr);for(const char *w:want)fprintf(stderr,"  [%s]\n",w);
    assert(false);
}
static bool has(const Lines &got,const std::string &row){for(const auto &g:got)if(g==row)return true;return false;}
static VmFailRecord record(uint8_t code,uint32_t detail){VmFailRecord r{};vm_fail_fill(r,code,detail);return r;}

int main(int argc,char **argv){
    assert(argc==3);base=argv[2];
    assert(base.string().find("packages-sandbox")!=std::string::npos);
    fs::create_directories(base);
    fs::copy(fs::path(argv[1]),base,fs::copy_options::recursive|fs::copy_options::overwrite_existing);

    VmHostId host{VM_HOSTID_MAGIC,VM_ABI,VM_HOST_SERVICES,0x8000,"TeensyROM",0};

    // The fixture, sorted by id, against a host that states no code floor: VENDOR asks
    // for a service no host here serves, and WIDE is linked below the window such a
    // host takes. Both get a '!' and say why, in tryLaunch's own terms.
    expect(rows(&host),{
        "  HELLO      hi",
        "  OTHER      ot",
        " !VENDOR     vn",
        " !WIDE       wd",
        " !VENDOR: host lacks service $10000",
        " !WIDE: host takes code at $18000 only"});
    // A host linked clear of the lower window takes WIDE.
    host.code_floor=0xdd1c;
    expect(rows(&host),{"  HELLO      hi","  OTHER      ot"," !VENDOR     vn","  WIDE       wd",
                        " !VENDOR: host lacks service $10000"});
    // One whose code fills it is told apart from one that states nothing.
    host.code_floor=VM_CODE_BASE;
    assert(has(rows(&host)," !WIDE: code $10000 below host $18000"));
    host.code_floor=0;

    // A host without a descriptor is launched without those checks, so nothing is
    // flagged; one on another ABI refuses everything, said once rather than per package.
    expect(rows(nullptr),{"  HELLO      hi","  OTHER      ot","  VENDOR     vn","  WIDE       wd"});
    VmHostId other=host;other.abi=VM_ABI+1;
    expect(rows(&other),{"  HELLO      hi","  OTHER      ot","  VENDOR     vn","  WIDE       wd",
                         " Host is ABI 3, firmware needs 2."});
    expect(rows(nullptr,false),{"  HELLO      hi","  OTHER      ot","  VENDOR     vn","  WIDE       wd",
                                " No host installed, so none launch."});

    // The last launch takes whatever row is left, in plain words for a launch that
    // started and in the menu's own for the rest, with the detail as the menu shows it.
    const VmFailRecord ok=record(VmFail::Ok,0),exited=record(VmFail::Exited,1),faulted=record(VmFail::Faulted,0),
                       bank=record(VmFail::ClientBank,3),installed=record(VmFail::Installed,0x8000);
    assert(rows(nullptr,true,&ok).back()==" Msg: extension started");
    assert(rows(nullptr,true,&exited).back()==" Msg: extension exited ($1)");
    assert(rows(nullptr,true,&faulted).back()==" Msg: extension crashed");
    assert(rows(nullptr,true,&bank).back()==" Msg: client CRT bank bad ($3)");
    assert(rows(nullptr,true,&installed).back()==" Msg: extension host installed");

    // Two packages claiming one extension: both marked, and one row naming both under
    // the first. OTHER2 has no module either, which is said too; '*' is the mark it gets.
    manifest("OTHER2","hi,zz");
    expect(rows(&host),{
        " *HELLO      hi",
        "  OTHER      ot",
        " *OTHER2     hi,zz",
        " !VENDOR     vn",
        " !WIDE       wd",
        " *.hi claimed by HELLO and OTHER2",
        " !OTHER2: module file bad or missing",
        " !VENDOR: host lacks service $10000",
        " !WIDE: host takes code at $18000 only"});
    // A third claimant, counted; the case of an extension does not make it another.
    manifest("THIRD","HI");
    {const Lines got=rows(nullptr);assert(has(got," *.hi claimed by 3 packages")&&has(got," *THIRD      HI"));}
    fs::remove_all(base/"VMS/OTHER2");fs::remove_all(base/"VMS/THIRD");

    // Folders find() skips without a word: a manifest that does not parse (here one
    // claiming a stock type), none at all, and a name that is no id, filtered to what
    // an id may hold and cut with a '>' when it is longer than the column.
    manifest("BADPRG","prg");
    fs::create_directories(base/"VMS/EMPTY");
    fs::create_directories(base/"VMS/My NES");
    fs::create_directories(base/"VMS"/std::string(30,'L'));
    {const Lines got=rows(nullptr);
     assert(has(got," xBADPRG     (no valid manifest.vmi)")&&has(got," xEMPTY      (no valid manifest.vmi)"));
     assert(has(got," xMy?NES     (bad folder name)")&&has(got," xLLLLLLLLL> (bad folder name)"));}
    fs::remove_all(base/"VMS/BADPRG");fs::remove_all(base/"VMS/EMPTY");
    fs::remove_all(base/"VMS/My NES");fs::remove_all(base/"VMS"/std::string(30,'L'));

    // A full-length id and a full-length list: the id cut at the column, the list after
    // its last whole extension that fits, each with a '>'. The problem row is cut at the
    // screen's last column the same way.
    const std::string longId(23,'L');
    manifest(longId,"a26,a52,a78,nes,gb,gbc,gg,sms,x");
    {const Lines got=rows(nullptr);
     assert(has(got," !LLLLLLLLL> a26,a52,a78,nes,gb,gbc,gg>"));
     assert(has(got," !LLLLLLLLLLLLLLLLLLLLLLL: module file>"));}
    fs::remove_all(base/"VMS"/longId);

    // Sorted ignoring case, as the card's own order is creation order.
    manifest("alpha","al");manifest("Beta","be");
    {const Lines got=rows(nullptr);assert(got[0].substr(2,5)=="alpha"&&got[1].substr(2,4)=="Beta");}
    fs::remove_all(base/"VMS/alpha");fs::remove_all(base/"VMS/Beta");

    // More packages than rows: five, a count of the rest, then as many reasons as are left.
    // The packages give way, never the reasons or the last launch: what does not fit is
    // counted on a row of its own.
    for(unsigned i=0;i<6;i++)manifest("PKG"+std::to_string(i),"p"+std::to_string(i));
    {const Lines got=rows(&host);
     assert(got.size()==Rows&&got[4]==" !PKG2       p2"&&got[5]=="   +5 more");
     assert(got[6]==" !PKG0: module file bad or missing"&&got[7]==" !PKG1: module file bad or missing");
     assert(got[8]=="   +6 more notes");}
    {const VmFailRecord crashed=record(VmFail::Faulted,0);
     const Lines got=rows(&host,true,&crashed);
     assert(got.size()==Rows&&got[5]=="   +5 more"&&got[6]==" !PKG0: module file bad or missing"&&
            got[7]=="   +7 more notes"&&got[8]==" Msg: extension crashed");}
    for(unsigned i=0;i<6;i++)fs::remove_all(base/"VMS"/("PKG"+std::to_string(i)));

    // Over the limit, every launch is refused; the list says so after what it read.
    for(unsigned i=0;i<VmRegistry::FolderLimit+1;i++)fs::create_directories(base/"VMS"/("PAD"+std::to_string(i)));
    assert(has(rows(nullptr)," Over 32 folders in /VMS: none launch."));
    for(unsigned i=0;i<VmRegistry::FolderLimit+1;i++)fs::remove_all(base/"VMS"/("PAD"+std::to_string(i)));
    failDirError=true;assert(has(rows(nullptr)," /VMS read error: none launch."));failDirError=false;

    // Nothing to list: an empty /VMS, none at all, and no card. The last is the caller's
    // word, not a read -- a pulled card still opens the root SdFat remembers -- so it is
    // said even with /VMS readable, and nothing is read.
    fs::rename(base/"VMS",base/"VMS.away");fs::create_directories(base/"VMS");
    expect(rows(&host),{" No packages in /VMS."});
    fs::remove_all(base/"VMS");
    expect(rows(&host),{" No /VMS folder on the SD card."});
    fs::rename(base/"VMS.away",base/"VMS");
    const unsigned scans=vmsOpens;
    expect(rows(&host,true,nullptr,false),{" No SD card, or it can't be read."});
    assert(vmsOpens==scans);
    expect(rows(nullptr),{"  HELLO      hi","  OTHER      ot","  VENDOR     vn","  WIDE       wd"});

    puts("PASS: package list over packager output; sorted rows with their marks, a module the host "
         "cannot serve and one linked below its window, no descriptor, another ABI and no host, shared "
         "extensions by two and three packages, unparsed manifests and names that are no id, both cuts, "
         "seven rows and a count, over the limit, a read error, no packages, no /VMS and no card, and "
         "the last launch in each of its forms");
}
