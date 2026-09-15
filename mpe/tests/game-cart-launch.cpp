// SPDX-License-Identifier: MIT
// Actual text-browser prefix, extension table, parser and minimal boot dispatch.
#define rmtSD FixtureSdSource
#include "../../vm/tests/fake_sd.h"
#undef rmtSD
#include "../../Source/Teensy/MinimalBoot/Common/Menu_Regs.h"
#include "../../Source/Teensy/MinimalBoot/Common/MPELaunch.h"
#include "game-cart-text-under-test.h"

static unsigned assertions;
static void check(bool ok,const char *why){++assertions;if(!ok){fprintf(stderr,"FAIL: %s (%s)\n",why,message.c_str());abort();}}
static uint8_t registers[IO1Size];static volatile uint8_t *IO1=registers;
static StructMenuItem *MenuSource;static uint16_t SelItemFullIdx;
static char DriveDirPath[256];static unsigned fallbacks;
#include "game-cart-browser-under-test.h"

static bool sdAvailable=true;static unsigned sdStarts;
static constexpr int BUILTIN_SDCARD=0,MinBootInd_FromMin=2;
static struct {Files sdfs;bool begin(int){++sdStarts;return sdAvailable;}} BootSD;
#define SD BootSD
#include "../../Source/Teensy/MinimalBoot/Common/MPEGameCartBoot.h"
static bool imageInstalled=true;static unsigned hostStarts,menuReturns,ordinaryBoots;
struct HostEntered{};struct MenuEntered{};
static void runMPEApp(){if(imageInstalled){++hostStarts;throw HostEntered{};}}
static void runMainTRApp_FromMin(){++menuReturns;throw MenuEntered{};}
#include "game-cart-boot-under-test.h"
#undef SD

static void reset(){message.clear();marker.clear();rebooted=false;failEEPRead=false;EEPROM.value=0;EEPROM.writes=0;}
static bool dispatch(uint8_t source,const char *directory,const char *name,bool isDirectory=false){
  std::string mutableName=name;StructMenuItem item{};item.Name=mutableName.data();
  item.ItemType=isDirectory?rtDirectory:Assoc_Ext_ItemType(item.Name);
  MenuSource=&item;strcpy(DriveDirPath,directory);IO1[rWRegCurrMenuWAIT]=source;
  const unsigned previous=fallbacks;HandleExecution();return previous==fallbacks;
}
static void boot(const char *path){marker=path;EEPROM.value=MinBootInd_ExecuteMin;try{bootUnderTest();}catch(HostEntered&){}catch(MenuEntered&){} }
static void write(const std::string &name,const std::vector<uint8_t>&bytes){const auto path=base/name;fs::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);out.write((const char*)bytes.data(),bytes.size());check(bool(out),"fixture written");}
int main(int argc,char **argv){
  check(argc==2,"isolated sandbox supplied");base=fs::absolute(argv[1]);
  std::ifstream in(base/"bundle.MPE",std::ios::binary);const std::vector<uint8_t> valid{std::istreambuf_iterator<char>(in),{}};
  check(valid.size()>VmGameCart::PrefixBytes,"synthetic fixture supplied");
  check(!VmRegistry::validExtensions("mpe")&&!VmRegistry::validExtensions("MPE,nes"),"registry cannot claim MPE");
  for(const char*ext:{"MPE","mpe","MpE","CRT","crt"}){
    std::string name=std::string("Game.")+ext,lower=std::string("Game.")+(!strcasecmp(ext,"crt")?"crt":"mpe");write("Nested Folder/"+lower,valid);reset();
    check(dispatch(rmtSD,"/Nested Folder",name.c_str()),"text browser handles MGC1 new/legacy suffix");
    check(rebooted&&EEPROM.value==MinBootInd_ExecuteMin&&marker=="/Nested Folder/"+lower&&message.empty(),"main commits exact checked path");
    check(!fs::exists(base/"VMS"),"no registry or extracted game required");
    const unsigned before=hostStarts;const std::string requested=marker;boot(requested.c_str());
    check(hostStarts==before+1&&EEPROM.value==MinBootInd_FromMin&&marker==requested,"minimal consumes request and preserves host/save path");
  }
  // Verify compressed content using the public reader actually used by preflight.
  {auto file=SD.sdfs.open("/Nested Folder/Game.mpe",O_RDONLY);VmGameCart::Reader reader;
    check(reader.open(&file,VmGameCartLaunch::readAt,uint32_t(file.fileSize())),"open actual bounded reader");
    const int content=reader.find("content.bin");std::vector<uint8_t> data(6500);
    check(content>=0&&reader.readEntry(content,0,data.data(),data.size())&&reader.verifyEntry(content),"independent compressed blocks and CRCs");
    bool correct=true;for(auto byte:data)correct&=byte==0x51;check(correct,"all 6500 decompressed synthetic bytes");}
  for(unsigned mib:{1u,2u}){std::vector<uint8_t> ordinary(mib*1024*1024);memcpy(ordinary.data(),"C64 CARTRIDGE   ",16);write("ordinary.crt",ordinary);reset();
    check(!dispatch(rmtSD,"/","ordinary.crt")&&!rebooted&&marker.empty(),"ordinary large CRT retains text path");
    const unsigned before=ordinaryBoots,sdBefore=sdStarts;boot("/ordinary.crt");check(ordinaryBoots==before+1&&sdStarts==sdBefore+1,"ordinary boot initializes SD once and keeps fallback");
    write("renamed.mpe",ordinary);reset();check(dispatch(rmtSD,"/","renamed.mpe")&&!rebooted&&!message.empty(),"renamed ordinary CRT is not MPE");}
  for(const char*name:{"missing.mpe","truncated.mpe","badcrc.mpe","badmodule.mpe","badversion.crt"}){
    auto damaged=valid;if(!strcmp(name,"truncated.mpe"))damaged.resize(0x4072);else if(!strcmp(name,"badmodule.mpe"))damaged.back()^=1;
    else if(!strcmp(name,"badversion.crt"))damaged[0x4073]='9';else damaged[0x4079]^=1;
    if(strcmp(name,"missing.mpe"))write(name,damaged);reset();check(dispatch(rmtSD,"/",name)&&!rebooted&&marker.empty()&&!message.empty(),"missing/corrupt/unsupported container fails closed");}
  reset();failEEPRead=true;check(dispatch(rmtSD,"/Nested Folder","Game.mpe")&&!rebooted&&message=="Cartridge launch path write failed","EEPROM verification gates reset");
  for(uint8_t source:{uint8_t(rmtUSBDrive),uint8_t(rmtTeensy)}){reset();check(dispatch(source,"/","Game.mpe")&&!rebooted&&!message.empty(),"unsupported source rejects MPE");check(!dispatch(source,"/","ordinary.crt"),"ordinary non-SD CRT unaffected");}
  reset();check(dispatch(rmtSD,"/disk.d64*","Game.mpe")&&!rebooted&&!message.empty(),"virtual disk MPE cannot reach ordinary loader");
  reset();check(!dispatch(rmtSD,"/","Folder.mpe",true),"directory with MPE suffix remains a directory");
  reset();std::string longPath="/"+std::string(249,'a');check(dispatch(rmtSD,longPath.c_str(),"Game.mpe")&&!rebooted,"oversized full path rejected");
  imageInstalled=false;const unsigned priorMenu=menuReturns;boot("/Nested Folder/Game.mpe");check(menuReturns==priorMenu+1&&EEPROM.value==MinBootInd_FromMin,"missing third image returns to menu without autolaunch");imageInstalled=true;
  const unsigned before=hostStarts;boot("@VM1");check(hostStarts==before+1,"installed VM launch still uses existing dispatch");
  char badPath[256];memset(badPath,'a',sizeof badPath);badPath[0]='/';bool initialized=false;
  check(!MPEGameCartBoot::requested(badPath,initialized)&&!initialized,"unterminated EEPROM path cannot be read as cartridge");
  printf("PASS: %u MPE text-browser/parser/boot assertions; no hardware or VM gameplay execution\n",assertions);
}
