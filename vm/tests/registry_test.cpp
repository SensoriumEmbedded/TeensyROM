#include <cassert>
#include <cstdio>
#include <cstring>
#include <strings.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>
namespace fs=std::filesystem;
#define FLASHMEM
enum { O_RDONLY=0,O_WRONLY=1,O_CREAT=2,O_TRUNC=4,rmtSD=1,eepAdCrtBootName=100,eepAdMinBootInd=2,MinBootInd_ExecuteMin=1 };
static fs::path base;
struct FsFile {
    fs::path path;std::shared_ptr<std::fstream> file;
    std::vector<fs::path> entries;size_t index=0;bool valid=false,dir=false;
    operator bool()const{return valid;}
    bool isDirectory()const{return dir;}
    uint64_t fileSize()const{return dir?0:fs::file_size(path);}
    int read(void *p,unsigned n){if(!file)return -1;file->read((char *)p,n);return file->gcount();}
    unsigned write(const void *p,unsigned n){file->write((const char *)p,n);return *file?n:0;}
    bool sync(){file->flush();return !!*file;}
    bool seekSet(uint32_t o){file->clear();file->seekg(o);return !!*file;}
    bool getError()const{return false;}
    bool close(){if(file)file->close();valid=false;return true;}
    size_t getName(char *p,size_t n){auto s=path.filename().string();if(s.size()>=n)return n;strcpy(p,s.c_str());return s.size();}
    bool openNext(FsFile *d,int){if(d->index==d->entries.size())return false;path=d->entries[d->index++];dir=fs::is_directory(path);valid=true;return true;}
};
struct Files {FsFile open(const char *p,int flags){
    FsFile f;
    f.path=base/fs::path(p).relative_path();if(!(flags&O_CREAT)&&!fs::exists(f.path))return f;
    f.dir=fs::is_directory(f.path);f.valid=true;if(f.dir){for(auto &e:fs::directory_iterator(f.path))f.entries.push_back(e.path());}
    else{f.file=std::make_shared<std::fstream>(f.path,std::ios::binary|((flags&O_WRONLY)?(std::ios::out|std::ios::trunc):std::ios::in));f.valid=!!*f.file;}return f;
}};
static struct {Files sdfs;} SD;
static bool rebooted;static std::string message,marker;
static void SendMsgPrintfln(const char *m){message=m;}
static void EEPwriteStr(int,const char *m){marker=m;}
static struct {void write(int,int){}} EEPROM;
static void delay(unsigned){}
#define SetResetAssert ((void)0)
#define REBOOT rebooted=true
#include "../../Source/Teensy/MinimalBoot/Common/VMRegistry.h"
static void put(const fs::path &p,const std::string &s){fs::create_directories(p.parent_path());std::ofstream(p,std::ios::binary)<<s;}
int main(int argc,char **argv){
    assert(argc==3);base=argv[2];assert(base.string().find("registry-sandbox")!=std::string::npos);
    fs::create_directories(base);fs::copy(fs::path(argv[1])/"VMS",base/"VMS",fs::copy_options::recursive|fs::copy_options::overwrite_existing);
    fs::copy_file(fs::path(argv[1])/"NESVM.crt",base/"NESVM.crt",fs::copy_options::overwrite_existing);
    using namespace VmRegistry;Launch l{};assert(find("nes",nullptr,l)==1);assert(!strcmp(l.root,"/VMS/NESVM"));assert(preflight(l));
    assert(validExtensions("gb,gbc")&&extensionMatches("gb,gbc","GB")&&extensionMatches("gb,gbc","GBC"));
    assert(!extensionMatches("gb,gbc","g")&&!validExtensions("gb,crt")&&!validExtensions("gb,")&&!validExtensions(",gb"));
    Manifest m{};assert(readManifest(l.root,m));refresh(true);assert(associated("Test.NES"));assert(!associated("Test.nes.exe"));refresh(false);assert(!associated("Test.NES"));
    assert(!absolute("/VMS/../secret",80));assert(!component("../NES"));assert(!component("NES/VM"));
    assert(tryLaunch(rmtSD,"/","NESVM.crt"));assert(rebooted&&marker=="@VM1");Launch saved{};assert(consume(saved)&&!saved.content[0]);
    rebooted=false;assert(tryLaunch(rmtSD,"/VMS/NESVM/ROMS","Crossbow.nes"));assert(rebooted);assert(consume(saved));assert(!strcmp(saved.content,"/VMS/NESVM/ROMS/Crossbow.nes"));
    put(base/"VMS/OTHER/manifest.vmi","VM1\nOTHER\nnes\nengine.mvm\nclient.crt\nEND\n");assert(find("nes",nullptr,l)==-1);
    put(base/"VMS/OTHER/manifest.vmi","VM1\nOTHER\nother\nengine.mvm\nclient.crt\nEND\n");assert(find("nes",nullptr,l)==1);
    put(base/"VMS/OTHER/manifest.vmi","VM1\nOTHER\nother\n../bad\nclient.crt\nEND\n");assert(!readManifest("/VMS/OTHER",m));
    auto module=base/"VMS/NESVM/engine.mvm";std::fstream damage(module,std::ios::binary|std::ios::in|std::ios::out);damage.seekg(64);const int byte=damage.get();damage.seekp(64);damage.put(byte^1);damage.close();assert(!preflight(l));
    rebooted=false;assert(tryLaunch(rmtSD,"/","NESVM.crt"));assert(!rebooted&&!message.empty());
    puts("PASS: real registry/preflight, generic .nes routing, client launch, one-shot record, duplicate extension, traversal, malformed manifest and corrupt module rejection");
}
