#pragma once
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <strings.h>
#include <climits>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>
namespace fs=std::filesystem;
#define FLASHMEM
enum { O_RDONLY=1,O_WRONLY=2,O_RDWR=3,O_CREAT=4,O_EXCL=8,O_TRUNC=16,T_WRITE=1,
 rmtSD=1,eepAdCrtBootName=100,eepAdMinBootInd=2,MinBootInd_ExecuteMin=1 };
static fs::path base;static bool failWrite,failFlush,failDirError;
static unsigned vmsOpens;   // how often the registry actually scanned /VMS
struct FsFile {
 fs::path path;std::shared_ptr<std::fstream> file;std::vector<fs::path> entries;
 size_t index=0;bool valid=false,dir=false;int flags=0;
 operator bool()const{return valid;}
 bool isDirectory()const{return dir;}
 uint64_t fileSize()const{return dir?0:fs::file_size(path);}
 bool isReadOnly()const{return false;}bool isHidden()const{return false;}
 int read(void *p,unsigned n){if(!file||!(flags&1))return -1;file->read((char *)p,n);return file->gcount();}
 unsigned write(const void *p,unsigned n){if(!file||!(flags&2)||failWrite)return 0;file->write((const char *)p,n);file->flush();return *file?n:0;}
 bool sync(){if(failFlush||!file)return false;file->clear();file->flush();return !!*file;}
 bool seekSet(uint32_t off){if(!file||off>fileSize())return false;file->clear();file->seekg(off);if(flags&2)file->seekp(off);return !!*file;}  //FatFile::seekSet refuses a seek past EOF
 bool getError()const{return (dir&&failDirError)||(file&&file->bad());}
 bool close(){bool ok=true;if(file&&file->is_open()){file->close();ok=!file->bad();}valid=false;return ok;}
 bool getModifyDateTime(uint16_t *d,uint16_t *t){*d=33;*t=0;return valid;}
 bool timestamp(int,unsigned year,unsigned month,unsigned day,unsigned h,unsigned m,unsigned s){return file&&(flags&2)&&year>=1980&&month>=1&&month<=12&&day>=1&&day<=31&&h<24&&m<60&&s<60;}
 bool truncate(uint32_t n){if(!file||!(flags&2))return false;file->flush();std::error_code e;fs::resize_file(path,n,e);return !e;}
 size_t getName(char *p,size_t n){auto s=path.filename().string();if(s.size()>=n)return n;strcpy(p,s.c_str());return s.size();}
 bool openNext(FsFile *d,int mode){if(d->index==d->entries.size())return false;path=d->entries[d->index++];dir=fs::is_directory(path);valid=true;flags=mode;
  if(!dir){file=std::make_shared<std::fstream>(path,std::ios::binary|std::ios::in);valid=!!*file;}return valid;}
};
struct Files {
 FsFile open(const char *p,int flags){if(!strcmp(p,"/VMS"))++vmsOpens;
  FsFile f;f.path=base/fs::path(p).relative_path();bool exists=fs::exists(f.path);
  if((!exists&&!(flags&O_CREAT))||(exists&&(flags&O_EXCL)))return f;
  f.dir=exists&&fs::is_directory(f.path);f.flags=flags;
  if(f.dir){for(auto &e:fs::directory_iterator(f.path))f.entries.push_back(e.path());f.valid=true;}
  else{if(!exists)std::ofstream(f.path,std::ios::binary).close();auto mode=std::ios::binary|std::ios::in;if(flags&2)mode|=std::ios::out;if(flags&O_TRUNC)mode|=std::ios::trunc;
   f.file=std::make_shared<std::fstream>(f.path,mode);f.valid=!!*f.file;}return f;
 }
 uint32_t clusterCount(){return 1024;}uint32_t freeClusterCount(){return 512;}uint32_t sectorsPerCluster(){return 8;}
 bool mkdir(const char *p,bool parents){std::error_code e;auto a=base/fs::path(p).relative_path();return parents?fs::create_directories(a,e):fs::create_directory(a,e);}
 bool remove(const char *p){auto a=base/fs::path(p).relative_path();std::error_code e;return !fs::is_directory(a)&&fs::remove(a,e);}
 bool rmdir(const char *p){auto a=base/fs::path(p).relative_path();std::error_code e;return fs::is_directory(a)&&fs::is_empty(a)&&fs::remove(a,e);}
 bool rename(const char *p,const char *q){std::error_code e;auto a=base/fs::path(p).relative_path(),b=base/fs::path(q).relative_path();if(fs::exists(b))return false;fs::rename(a,b,e);return !e;}
};
static struct {Files sdfs;} SD;
static bool rebooted;static std::string message,marker;
// Varargs like the firmware's, so a test sees the message the C64 would, and
// -Wformat checks the firmware's format strings against their arguments.
__attribute__((format(printf,1,2)))
static void SendMsgPrintfln(const char *fmt,...){
    char buffer[160];va_list args;va_start(args,fmt);
    vsnprintf(buffer,sizeof buffer,fmt,args);va_end(args);message=buffer;}
static void EEPwriteStr(int,const char *m){marker=m;}
static struct {void write(int,int){}} EEPROM;
static void delay(unsigned){}
#define SetResetAssert ((void)0)
#define REBOOT rebooted=true
