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

    // An ordinary cartridge is still an ordinary cartridge, at any size. File
    // size is not a VM identity; only the descriptor at 0x4070 is.
    for(unsigned mib:{1u,2u}){
        std::vector<char> crt(mib*1024*1024,0);
        memcpy(crt.data(),"C64 CARTRIDGE   ",16);
        std::ofstream file(base/"ordinary.crt",std::ios::binary);
        file.write(crt.data(),crt.size());file.close();
        assert(!VmLaunch::tryFile(rmtSD,"/","ordinary.crt")&&!rebooted);
    }

    // A cartridge that claims to be ours but is damaged reports, and does not boot.
    auto damaged=base/"HELLO.crt";
    std::fstream f(damaged,std::ios::binary|std::ios::in|std::ios::out);
    f.seekp(0x4074);f.put(99);f.close();   // ABI byte in the descriptor
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

    puts("PASS: client and content launch, 12 stock file types and non-SD sources fall through untouched, "
         "1/2 MiB ordinary CRTs keep stock parsing, damaged and orphaned clients report without booting, "
         "cached extension table skipping the scan, deferring to it when over the limit or errored, and its staleness bound, "
         "boot-image bounds");
}
