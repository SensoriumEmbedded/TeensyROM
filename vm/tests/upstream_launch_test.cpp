// SPDX-License-Identifier: MIT
#include "fake_sd.h"
#include "../../Source/Teensy/MinimalBoot/Common/MPELaunch.h"
#include "../../Source/Teensy/MinimalBoot/Common/MPEBootImage.h"

int main(int argc,char **argv){
    assert(argc==3||argc==4);base=argv[2];const bool all=argc==4&&!strcmp(argv[3],"all");
    assert(base.string().find("launch-sandbox-")!=std::string::npos);
    fs::create_directories(base);
    fs::copy(fs::path(argv[1])/"VMS",base/"VMS",fs::copy_options::recursive);
    const char *ids[]={"DOOMVM","NESVM","DOSVM","AGIVM","GBVM"};
    const char *extensions[]={"gbd","nes","img","agi","gbc"};
    for(unsigned i=0;i<(all?5u:1u);i++){
        const auto file=std::string(ids[i])+".crt";
        fs::copy_file(base/"VMS"/ids[i]/"client.crt",base/file);
        rebooted=false;message.clear();
        assert(MPELaunch::tryFile(rmtSD,"/",file.c_str())&&rebooted&&message.empty());
        VmRegistry::Launch launch{};assert(VmRegistry::consume(launch));
        assert(std::string(launch.root)==std::string("/VMS/")+ids[i]&&launch.content[0]==0);
        rebooted=false;const auto content=std::string("Example.")+extensions[i];
        assert(MPELaunch::tryFile(rmtSD,"/Games",content.c_str())&&rebooted);
        assert(VmRegistry::consume(launch)&&std::string(launch.content)=="/Games/"+content);
    }
    rebooted=false;
    for(const char *name:{"GAME.PRG","music.SID","disk.D64","update.hex","notes.txt","plain","other.unknown"})
        assert(!MPELaunch::tryFile(rmtSD,"/",name));
    assert(!MPELaunch::tryFile(2,"/","NESVM.crt")); // USB keeps stock routing.
    assert(!MPELaunch::tryFile(rmtSD,"/disk.d64*","NESVM.crt"));
    assert(!MPELaunch::tryFile(rmtSD,"","NESVM.crt"));
    assert(!MPELaunch::tryFile(rmtSD,nullptr,"NESVM.crt"));
    assert(!rebooted);
    // Large ordinary CRTs retain stock parsing; file size is not a VM identity.
    for(unsigned mib:{1u,2u}){
        std::vector<char> crt(mib*1024*1024,0);memcpy(crt.data(),"C64 CARTRIDGE   ",16);
        std::ofstream file(base/"ordinary.crt",std::ios::binary);file.write(crt.data(),crt.size());file.close();
        assert(!MPELaunch::tryFile(rmtSD,"/","ordinary.crt")&&!rebooted);
    }
    auto bad=base/"DOOMVM.crt";std::fstream f(bad,std::ios::binary|std::ios::in|std::ios::out);
    f.seekp(0x4074);f.put(99);f.close();message.clear();
    assert(MPELaunch::tryFile(rmtSD,"/","DOOMVM.crt")&&!rebooted&&!message.empty());
    fs::copy_file(base/"VMS/DOOMVM/client.crt",bad,fs::copy_options::overwrite_existing);
    fs::rename(base/"VMS/DOOMVM",base/"saved-DOOMVM");message.clear();
    assert(MPELaunch::tryFile(rmtSD,"/","DOOMVM.crt")&&!rebooted&&!message.empty());
    using MPEBootImage::valid;
    const auto address=MPEBootImage::base;
    assert(valid(0x42464346,0x432000d1,address+0x1801,address,123904));
    assert(!valid(0xffffffff,0x432000d1,address+0x1801,address,123904));
    assert(!valid(0x42464346,0xffffffff,address+0x1801,address,123904));
    assert(!valid(0x42464346,0x432000d1,address+0x1800,address,123904));
    assert(!valid(0x42464346,0x432000d1,0x60001801,address,123904));
    assert(!valid(0x42464346,0x432000d1,address+0x1801,0x60000000,123904));
    assert(!valid(0x42464346,0x432000d1,address+0x1801,address,MPEBootImage::limit-address+1));
    printf("PASS: %s launcher/direct association, ordinary 1/2 MiB CRT fallthrough, protected extensions, non-SD routing, corrupt/missing packages and relocated boot-image bounds\n",all?"five development packages":"DoomVM");
}
