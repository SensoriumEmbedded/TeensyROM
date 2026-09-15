// SPDX-License-Identifier: MIT
#include "../../vm/tests/fake_sd.h"
#include "../../Source/Teensy/MinimalBoot/Common/MPELaunch.h"

// Compile the actual two bus helpers with observable GPIO/time operations.
static uint32_t nS_DMADataSetup, nS_DMADataHold, gpioRead, gpioSet, gpioClear;
static uint32_t waited;
static bool bufferOut, portOut, drivenDuringWait;
static void waitFine(uint32_t value) { waited=value; drivenDuringWait=bufferOut&&portOut; }
#define WaitUntil_nS_fine(value) waitFine(value)
#define ReadGPIO7 gpioRead
#define CORE_PIN10_PORTSET gpioSet
#define CORE_PIN10_PORTCLEAR gpioClear
#define GP7_DataMask 0x000f000fu
#define SetDataBufOut (bufferOut=true)
#define SetDataBufIn (bufferOut=false)
#define SetDataPortDirOut (portOut=true)
#define SetDataPortDirIn (portOut=false)
#include "direct-console-dma-under-test.h"

static fs::path packages, sandbox;
static unsigned routeChecks;
enum class Result { Fallthrough, Rejected, Launch };
static void put(const fs::path &file,const std::string &text) {
    fs::create_directories(file.parent_path());std::ofstream(file,std::ios::binary)<<text;
}
static std::vector<uint8_t> bytes(const fs::path &file) {
    std::ifstream in(file,std::ios::binary);assert(in);
    return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
}
static void putBytes(const fs::path &file,const std::vector<uint8_t> &data) {
    std::ofstream out(file,std::ios::binary);out.write(reinterpret_cast<const char*>(data.data()),data.size());assert(out);
}
static void use(const char *name,bool installed=true) {
    base=sandbox/name;assert(!fs::exists(base));fs::create_directories(base);
    if(installed) {
        fs::copy(packages/"VMS",base/"VMS",fs::copy_options::recursive);
        for(const char *id:{"NESVM","DOOMVM","GBVM","GGVM"})
            fs::copy_file(base/"VMS"/id/"client.crt",base/(std::string(id)+".crt"));
    }
    failWrite=failFlush=false;
}
static void route(uint8_t source,const char *directory,const char *name,Result expected) {
    rebooted=false;message.clear();marker.clear();++routeChecks;
    const bool handled=MPELaunch::tryFile(source,directory,name);
    assert(handled==(expected!=Result::Fallthrough));
    assert(rebooted==(expected==Result::Launch));
    assert((expected==Result::Rejected)==!message.empty());
    assert(marker==(expected==Result::Launch?"@VM1":""));
}
static void saved(const char *root,const char *content) {
    VmRegistry::Launch launch{};assert(VmRegistry::consume(launch));
    assert(std::string(launch.root)==root&&std::string(launch.content)==content);
}
static void fixturePackage(const char *id,const char *extension) {
    const auto destination=base/"VMS"/id;fs::create_directories(destination);
    fs::copy_file(base/"VMS/NESVM/engine.mvm",destination/"engine.mvm");
    auto client=bytes(base/"VMS/NESVM/client.crt");
    std::memset(client.data()+0x4080,0,24);std::strcpy(reinterpret_cast<char*>(client.data()+0x4080),id);
    const uint32_t crc=vm_crc32(client.data()+0x4070,124);std::memcpy(client.data()+0x4070+124,&crc,4);
    putBytes(destination/"client.crt",client);
    put(destination/"manifest.vmi",std::string("VM1\n")+id+"\n"+extension+"\nengine.mvm\nclient.crt\nEND\n");
}
static void flip(const fs::path &file,unsigned offset) {
    auto data=bytes(file);assert(offset<data.size());data[offset]^=1;putBytes(file,data);
}
int main(int argc,char **argv) {
    assert(argc==3);packages=argv[1];sandbox=argv[2];
    assert(sandbox.filename().string().find("launch-sandbox-")==0);
    fs::create_directories(sandbox);
    use("released-packages");
    unsigned packagePreflights=0;
    for(const char *id:{"NESVM","DOOMVM","GBVM","GGVM"}) {
        VmRegistry::Launch launch{};assert(VmRegistry::find("",id,launch)==1);
        assert(VmRegistry::preflight(launch));++packagePreflights;
        const auto client=std::string(id)+".crt";
        route(rmtSD,"/",client.c_str(),Result::Launch);
        saved((std::string("/VMS/")+id).c_str(),"");
    }
    route(rmtSD,"/Games/Nested Folder","Selected.NES",Result::Launch);
    saved("/VMS/NESVM","/Games/Nested Folder/Selected.NES");
    route(rmtSD,"/Games/","doom1.gbd",Result::Launch);saved("/VMS/DOOMVM","/Games/doom1.gbd");

    // Real GB/GG engine, manifest and client bytes, including their actual
    // ABI/service requirements and descriptor CRCs. A renamed NES fixture
    // cannot prove that the released GB/GG packages are admissible.
    use("gb-color-aliases");
    for(const char *name:{"Game.GB","Game.GBC","Game.gC"}) {
        route(rmtSD,"/Nested Games",name,Result::Launch);
        saved("/VMS/GBVM",(std::string("/Nested Games/")+name).c_str());
    }
    route(rmtSD,"/Games","Game.GG",Result::Launch);saved("/VMS/GGVM","/Games/Game.GG");
    assert(VmRegistry::extensionMatches("gb,gc","GBC"));
    assert(VmRegistry::extensionMatches("gb,gbc","GC"));
    assert(!VmRegistry::extensionMatches("gb,gbc","gbc2"));
    assert(!VmRegistry::extensionMatches("gb,gbc","g"));
    VmRegistry::refresh(true);assert(VmRegistry::associated("Game.GC"));
    VmRegistry::refresh(false);assert(!VmRegistry::associated("Game.GC"));

    use("missing-packages",false);
    for(const char *name:{"Game.NES","Game.GB","Game.GBC","Game.GC","Game.GG"})
        route(rmtSD,"/",name,Result::Rejected);

    use("bad-manifest");put(base/"VMS/NESVM/manifest.vmi","VM1\nNESVM\nnes\nengine.mvm\nclient.crt\nBROKEN\n");
    route(rmtSD,"/Games","Game.nes",Result::Rejected);
    use("ambiguous");fixturePackage("OTHER","nes");route(rmtSD,"/Games","Game.nes",Result::Rejected);
    use("over-limit");for(unsigned i=0;i<31;i++)fs::create_directories(base/"VMS"/("EXTRA"+std::to_string(i)));
    route(rmtSD,"/Games","Game.nes",Result::Rejected);
    use("module-missing");fs::rename(base/"VMS/NESVM/engine.mvm",base/"saved-engine.mvm");
    route(rmtSD,"/Games","Game.nes",Result::Rejected);
    use("module-corrupt");flip(base/"VMS/NESVM/engine.mvm",64);route(rmtSD,"/Games","Game.nes",Result::Rejected);
    use("module-unsupported");{
        auto data=bytes(base/"VMS/NESVM/engine.mvm");VmImageHeader header{};std::memcpy(&header,data.data(),sizeof header);
        header.required_services|=0x80000000u;header.header_crc=0;header.header_crc=vm_crc32(&header,sizeof header);
        std::memcpy(data.data(),&header,sizeof header);putBytes(base/"VMS/NESVM/engine.mvm",data);
    }
    route(rmtSD,"/Games","Game.nes",Result::Rejected);
    use("client-corrupt");flip(base/"VMS/NESVM/client.crt",80);route(rmtSD,"/Games","Game.nes",Result::Rejected);
    use("client-missing");fs::rename(base/"VMS/NESVM/client.crt",base/"saved-client.crt");
    route(rmtSD,"/Games","Game.nes",Result::Rejected);
    use("descriptor-corrupt");flip(base/"NESVM.crt",0x4074);route(rmtSD,"/","NESVM.crt",Result::Rejected);
    use("launch-write-failure");failWrite=true;route(rmtSD,"/Games","Game.nes",Result::Rejected);
    use("launch-sync-failure");failFlush=true;route(rmtSD,"/Games","Game.nes",Result::Rejected);

    use("stock-fallthrough");
    for(const char *name:{"file.PRG","music.SID","disk.D64","firmware.hex","notes.txt","plain","other.smc","Game.nes.exe"})
        route(rmtSD,"/",name,Result::Fallthrough);
    route(2,"/","Game.nes",Result::Fallthrough);
    route(rmtSD,"/disk.d64*","Game.nes",Result::Fallthrough);
    route(rmtSD,"","Game.nes",Result::Fallthrough);
    route(rmtSD,nullptr,"Game.nes",Result::Fallthrough);
    route(rmtSD,"/",nullptr,Result::Fallthrough);
    for(unsigned mib:{1u,2u}) {
        std::vector<uint8_t> ordinary(mib*1024*1024);std::memcpy(ordinary.data(),"C64 CARTRIDGE   ",16);
        putBytes(base/"ordinary.crt",ordinary);route(rmtSD,"/","ordinary.crt",Result::Fallthrough);
    }
    std::string longDirectory="/"+std::string(250,'a');
    route(rmtSD,longDirectory.c_str(),"Game.nes",Result::Rejected);

    unsigned dmaCases=0;
    for(bool ntsc:{false,true}) {
        nS_DMADataSetup=ntsc?Def_nS_DMADataSetupNTSC:Def_nS_DMADataSetupPAL;
        nS_DMADataHold=ntsc?Def_nS_DMADataHoldNTSC:Def_nS_DMADataHoldPAL;
        assert(nS_DMADataSetup==390&&nS_DMADataHold==(ntsc?410u:430u));
        for(unsigned value=0;value<256;value++) {
            const uint32_t pins=(value&15u)|((value&240u)<<12);
            gpioRead=pins;assert(DataPortWaitReadDMA()==value);
            assert(waited==nS_DMADataSetup&&!drivenDuringWait);
            DataPortWriteWaitDMA(value);
            assert(waited==nS_DMADataHold&&drivenDuringWait&&!bufferOut&&!portOut);
            assert(gpioSet==pins&&gpioClear==((~pins)&GP7_DataMask));++dmaCases;
        }
    }
    std::printf("PASS: %u launch-route checks, %u released-package preflights, %u PAL/NTSC DMA byte cases; actual production functions, simulated SD/GPIO, no ROM emulation or hardware acceptance\n",routeChecks,packagePreflights,dmaCases);
}
