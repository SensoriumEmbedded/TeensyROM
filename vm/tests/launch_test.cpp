// SPDX-License-Identifier: MIT
// VmLaunch::tryFile is the single hook the stock menu calls. Everything the
// menu already owns must fall straight through it untouched -- that property,
// not the launching, is what makes this safe to put in the menu path.
#include "fake_sd.h"
#include "../../Source/Teensy/MinimalBoot/Common/VMLaunch.h"
#include "../../Source/Teensy/MinimalBoot/Common/VMBootImage.h"

int main(int argc,char **argv){
    assert(argc==3);base=argv[2];
    assert(base.string().find("launch-sandbox-")!=std::string::npos);
    fs::create_directories(base);
    fs::copy(fs::path(argv[1]),base,fs::copy_options::recursive);

    // Launching by client cartridge, then by associated content file.
    fs::copy_file(base/"VMS/HELLO/client.crt",base/"HELLO.crt",fs::copy_options::overwrite_existing);
    VmBootImage::install(VM_HOST_SERVICES);
    rebooted=false;message.clear();
    assert(VmLaunch::tryFile(rmtSD,"/","HELLO.crt")&&rebooted&&message.empty());
    VmRegistry::Launch launch{};
    assert(VmRegistry::consume(launch)&&!strcmp(launch.root,"/VMS/HELLO")&&!launch.content[0]);

    fs::create_directories(base/"Games");
    std::ofstream(base/"Games/Example.hi",std::ios::binary)<<"content";
    rebooted=false;
    assert(VmLaunch::tryFile(rmtSD,"/Games","Example.hi")&&rebooted);
    assert(VmRegistry::consume(launch)&&!strcmp(launch.content,"/Games/Example.hi"));

    // Everything the stock menu owns is not ours to touch. None of these may
    // even reach the registry, let alone reboot the machine.
    rebooted=false;
    for(const char *name:{"GAME.PRG","music.SID","disk.D64","disk.D71","disk.D81","update.hex",
                          "notes.txt","readme.MD","art.koa","dump.reu","plain","other.unknown"})
        assert(!VmLaunch::tryFile(rmtSD,"/",name));
    assert(!VmLaunch::tryFile(2,"/","HELLO.crt"));        // USB keeps stock routing.
    assert(!VmLaunch::tryFile(rmtSD,"/disk.d64*","HELLO.crt")); // inside a mounted image
    assert(!VmLaunch::tryFile(rmtSD,"","HELLO.crt"));
    assert(!VmLaunch::tryFile(rmtSD,nullptr,"HELLO.crt"));
    assert(!rebooted);

    // isPackageCrt, RemoteLaunch's test for sending a package through the menu before the
    // machine is known, answers the same question as tryFile without launching anything.
    assert(VmLaunch::isPackageCrt(rmtSD,"/","HELLO.crt")&&!rebooted);
    assert(!VmLaunch::isPackageCrt(2,"/","HELLO.crt"));          // USB keeps stock routing.
    assert(!VmLaunch::isPackageCrt(rmtSD,"/disk.d64*","HELLO.crt"));
    assert(!VmLaunch::isPackageCrt(rmtSD,"/","GAME.PRG"));
    assert(!VmLaunch::isPackageCrt(rmtSD,"","HELLO.crt")&&!VmLaunch::isPackageCrt(rmtSD,nullptr,"HELLO.crt"));
    assert(!rebooted);

    // An ordinary cartridge is still an ordinary cartridge, at any size. File
    // size is not a VM identity; only the descriptor at 0x4070 is.
    for(unsigned mib:{1u,2u}){
        std::vector<char> crt(mib*1024*1024,0);
        memcpy(crt.data(),"C64 CARTRIDGE   ",16);
        std::ofstream file(base/"ordinary.crt",std::ios::binary);
        file.write(crt.data(),crt.size());file.close();
        assert(!VmLaunch::tryFile(rmtSD,"/","ordinary.crt")&&!rebooted);
        assert(!VmLaunch::isPackageCrt(rmtSD,"/","ordinary.crt"));
    }

    // A cartridge that claims to be ours but is damaged reports, and does not boot.
    auto damaged=base/"HELLO.crt";
    std::fstream f(damaged,std::ios::binary|std::ios::in|std::ios::out);
    f.seekp(0x4074);f.put(99);f.close();   // ABI byte in the descriptor
    // Still a package as far as routing goes: it goes through the menu, where tryLaunch reports it.
    assert(VmLaunch::isPackageCrt(rmtSD,"/","HELLO.crt"));
    message.clear();
    assert(VmLaunch::tryFile(rmtSD,"/","HELLO.crt")&&!rebooted&&!message.empty());

    // A cartridge whose package is missing reports too, rather than falling
    // through to be parsed as an ordinary EasyFlash image.
    fs::copy_file(base/"VMS/HELLO/client.crt",damaged,fs::copy_options::overwrite_existing);
    fs::rename(base/"VMS/HELLO",base/"saved-HELLO");
    message.clear();
    assert(VmLaunch::tryFile(rmtSD,"/","HELLO.crt")&&!rebooted&&!message.empty());

    // The cached extension table stands in for the scan only where it can
    // answer. Refreshed with the listing, a file it does not know costs no scan,
    // and a file it does know still launches.
    fs::rename(base/"saved-HELLO",base/"VMS/HELLO");
    VmRegistry::refresh(true);
    vmsOpens=0;rebooted=false;message.clear();
    assert(!VmLaunch::tryFile(rmtSD,"/Games","Example.zz")&&!vmsOpens&&!rebooted&&message.empty());
    assert(VmLaunch::tryFile(rmtSD,"/Games","Example.hi")&&rebooted&&vmsOpens);

    // A table that could not be built must not deny. Over the limit, and on a
    // directory read error, the scan runs and refuses out loud instead.
    for(unsigned i=0;i<33;i++)fs::create_directories(base/"VMS"/("PAD"+std::to_string(i)));
    VmRegistry::refresh(true);
    rebooted=false;message.clear();
    assert(VmLaunch::tryFile(rmtSD,"/Games","Example.hi")&&!rebooted&&!message.empty());
    for(unsigned i=0;i<33;i++)fs::remove_all(base/"VMS"/("PAD"+std::to_string(i)));
    failDirError=true;
    VmRegistry::refresh(true);
    rebooted=false;message.clear();
    assert(VmLaunch::tryFile(rmtSD,"/Games","Example.hi")&&!rebooted&&!message.empty());
    failDirError=false;

    // Staleness is what the refresh points above exist to bound: a package that
    // appears after the listing was built is not seen until the next refresh.
    fs::rename(base/"VMS/HELLO",base/"saved-HELLO");
    VmRegistry::refresh(true);
    rebooted=false;
    assert(!VmLaunch::tryFile(rmtSD,"/Games","Example.hi")&&!rebooted);
    fs::rename(base/"saved-HELLO",base/"VMS/HELLO");
    assert(!VmLaunch::tryFile(rmtSD,"/Games","Example.hi")&&!rebooted);
    VmRegistry::refresh(true);
    assert(VmLaunch::tryFile(rmtSD,"/Games","Example.hi")&&rebooted);

    // The reserved flash slot the extension image boots from.
    using VmBootImage::valid;
    const auto address=VmBootImage::base;
    assert(valid(0x42464346,0x432000d1,address+0x1801,address,123904));
    assert(!valid(0xffffffff,0x432000d1,address+0x1801,address,123904));
    assert(!valid(0x42464346,0xffffffff,address+0x1801,address,123904));
    assert(!valid(0x42464346,0x432000d1,address+0x1800,address,123904));   // no Thumb bit
    assert(!valid(0x42464346,0x432000d1,0x60001801,address,123904));       // wrong slot
    assert(!valid(0x42464346,0x432000d1,address+0x1801,0x60000000,123904));
    assert(!valid(0x42464346,0x432000d1,address+0x1801,address,VmBootImage::limit-address+1));

    // Shift+S on a .sid: launchInHost boots the SIDHost extension with the file as content
    // and the host's folder as root, no package behind it. Any other host, or none, refuses by
    // name and nothing reboots; a selection that is not an SD .sid is not its to answer.
    // A v2 header, one song, the load address in the data ($1000), and an RTS to load there.
    auto sidImage=[](const char *magic){std::string s(0x7C,'\0');memcpy(&s[0],magic,4);s[5]=2;s[7]=0x7C;s[0x0F]=1;s[0x11]=1;
                                       return s+std::string("\x00\x10\x60",3);};
    auto put=[](const fs::path &p,const std::string &s){std::ofstream(p,std::ios::binary)<<s;};
    fs::create_directories(base/"Music/Hubbard");
    put(base/"Music/Hubbard/Commando.sid",sidImage("PSID"));
    VmBootImage::install(0,VM_ABI,"SIDHost");
    rebooted=false;message.clear();marker.clear();machineAt=-1;
    assert(VmLaunch::launchInHost(rmtSD,"//Music/Hubbard","Commando.sid")&&rebooted&&message.empty());
    assert(VmRegistry::consume(launch)&&!strcmp(launch.root,"/VMS/SIDHost")&&
           !strcmp(launch.content,"/Music/Hubbard/Commando.sid")&&!launch.manifest_crc);
    assert(marker==VM_HOST_MARKER&&machineAt==VM_EEP_MACHINE_ADDR);
    VmBootImage::install(VM_HOST_SERVICES);   // a module host, named TeensyROM, would take the .sid for a module
    rebooted=false;message.clear();
    assert(VmLaunch::launchInHost(rmtSD,"/Music/Hubbard","Commando.sid")&&!rebooted&&
           message.find("TeensyROM")!=std::string::npos&&message.find("SIDHost")!=std::string::npos);
    VmBootImage::installWithoutDescriptor();
    rebooted=false;message.clear();
    assert(VmLaunch::launchInHost(rmtSD,"/Music/Hubbard","Commando.sid")&&!rebooted&&message.find("(no descriptor)")!=std::string::npos);
    VmBootImage::erase();
    rebooted=false;message.clear();
    {bool badFile=true;   // a refusal of the host, not the file: the built-in player can still play it
     assert(VmLaunch::launchInHost(rmtSD,"/Music/Hubbard","Commando.sid",&badFile)&&!rebooted&&!message.empty()&&!badFile);}
    VmBootImage::install(0,VM_ABI+1,"SIDHost");   // the right name built against another ABI reads the record differently
    rebooted=false;message.clear();
    assert(VmLaunch::launchInHost(rmtSD,"/Music/Hubbard","Commando.sid")&&!rebooted&&message.find("ABI")!=std::string::npos);
    VmBootImage::install(0,VM_ABI,"SIDHost");
    // The file itself is checked before anything is written, against the header rules the host
    // refuses a file on: not a SID, empty, missing, or a header the host's parser would reject.
    put(base/"Music/Hubbard/Notes.sid","just text");
    put(base/"Music/Hubbard/Empty.sid","");
    put(base/"Music/Hubbard/Magic.sid","PSID");
    {std::string s=sidImage("PSID");s[5]=9;put(base/"Music/Hubbard/Version.sid",s);}
    {std::string s=sidImage("PSID");s[7]=0x76;put(base/"Music/Hubbard/Offset.sid",s);}
    {std::string s=sidImage("PSID");s[0x0F]=0;put(base/"Music/Hubbard/NoSongs.sid",s);}
    {std::string s=sidImage("PSID");s[0x77]=1;put(base/"Music/Hubbard/Mus.sid",s);}
    {std::string s=sidImage("RSID");s[8]=0x10;put(base/"Music/Hubbard/RsidLoad.sid",s);}
    {std::string s=sidImage("RSID");s[4]=0;s[5]=1;s[7]=0x76;s.erase(0x76,6);put(base/"Music/Hubbard/RsidV1.sid",s);}
    {std::string s=sidImage("PSID");s.resize(0x7E);put(base/"Music/Hubbard/NoData.sid",s);}
    {std::string s=sidImage("PSID");s[0x7C]='\xFF';s[0x7D]='\xFF';s+='\x60';put(base/"Music/Hubbard/Wraps.sid",s);}
    for(const char *bad:{"Notes.sid","Empty.sid","Missing.sid","Magic.sid","Version.sid","Offset.sid","NoSongs.sid",
                         "Mus.sid","RsidLoad.sid","RsidV1.sid","NoData.sid","Wraps.sid"}){
        rebooted=false;message.clear();marker.clear();bool badFile=false;
        assert(VmLaunch::launchInHost(rmtSD,"/Music/Hubbard",bad,&badFile)&&!rebooted&&marker.empty()&&
               message.find("PSID")!=std::string::npos&&badFile);}
    // A version 1 header, and a PSID that loads at the address its header names.
    {std::string s=sidImage("PSID");s[5]=1;s[7]=0x76;s.erase(0x76,6);put(base/"Music/Hubbard/V1.sid",s);}
    {std::string s=sidImage("PSID");s[8]=0x10;s.erase(0x7C,2);put(base/"Music/Hubbard/Header.sid",s);}
    for(const char *good:{"V1.sid","Header.sid"}){
        rebooted=false;message.clear();
        assert(VmLaunch::launchInHost(rmtSD,"/Music/Hubbard",good)&&rebooted&&message.empty());}
    put(base/"Music/Hubbard/Real.sid",sidImage("RSID"));
    rebooted=false;message.clear();
    assert(VmLaunch::launchInHost(rmtSD,"/Music/Hubbard","Real.sid")&&rebooted&&message.empty());
    // A card that never held a package has no /VMS, and the launch record lives there.
    fs::rename(base/"VMS",base/"saved-VMS");
    rebooted=false;message.clear();
    assert(VmLaunch::launchInHost(rmtSD,"/Music/Hubbard","Commando.sid")&&rebooted&&message.empty());
    assert(VmRegistry::consume(launch)&&!strcmp(launch.content,"/Music/Hubbard/Commando.sid"));
    fs::remove_all(base/"VMS");fs::rename(base/"saved-VMS",base/"VMS");
    // ".." in a name is legal on the card but not in the record: refused before the last
    // record is overwritten.
    put(base/"Music/Hubbard/Commando..sid",sidImage("PSID"));
    rebooted=false;message.clear();marker.clear();
    assert(VmLaunch::launchInHost(rmtSD,"/Music/Hubbard","Commando..sid")&&!rebooted&&marker.empty()&&
           message.find("..")!=std::string::npos);
    assert(VmRegistry::consume(launch)&&!strcmp(launch.content,"/Music/Hubbard/Real.sid"));
    rebooted=false;message.clear();
    assert(!VmLaunch::launchInHost(2,"/Music/Hubbard","Commando.sid"));        // USB keeps the built-in player
    assert(!VmLaunch::launchInHost(rmtSD,"/disk.d64*","Commando.sid"));        // inside a mounted image
    assert(!VmLaunch::launchInHost(rmtSD,"/Music/Hubbard","GAME.PRG"));
    assert(!VmLaunch::launchInHost(rmtSD,"","Commando.sid")&&!VmLaunch::launchInHost(rmtSD,nullptr,"Commando.sid"));
    assert(!rebooted&&message.empty());

    puts("PASS: client and content launch, 12 stock file types and non-SD sources fall through untouched, "
         "1/2 MiB ordinary CRTs keep stock parsing, damaged and orphaned clients report without booting, "
         "cached extension table skipping the scan, deferring to it when over the limit or errored, and its staleness bound, "
         "boot-image bounds, and the .sid launch into the SIDHost extension refusing every other host "
         "and every header its parser rejects, and making /VMS on a card without one, "
         "and refusing a path the record cannot carry");
}
