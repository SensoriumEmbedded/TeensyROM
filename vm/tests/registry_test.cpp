// SPDX-License-Identifier: MIT
// The real VMRegistry against real packages written by tools/lib/extension.mjs:
// manifest parsing, extension routing, preflight (which re-validates the module
// image AND the client cartridge's descriptor and bank CRCs), and the one-shot
// launch record.
#include "fake_sd.h"
#include "../../Source/Teensy/MinimalBoot/Common/VMRegistry.h"

static void put(const fs::path &p,const std::string &s){fs::create_directories(p.parent_path());std::ofstream(p,std::ios::binary)<<s;}

int main(int argc,char **argv){
    assert(argc==3);base=argv[2];
    assert(base.string().find("registry-sandbox")!=std::string::npos);
    fs::create_directories(base);
    fs::copy(fs::path(argv[1]),base,fs::copy_options::recursive|fs::copy_options::overwrite_existing);
    using namespace VmRegistry;

    Launch launch{};
    assert(find("hi",nullptr,launch)==1);
    assert(!strcmp(launch.root,"/VMS/HELLO"));
    // preflight re-reads the module image and the client cartridge end to end.
    assert(preflight(launch));

    // Comma lists are ordinary manifest data, not a per-package firmware case.
    assert(validExtensions("gb,gbc")&&extensionMatches("gb,gbc","GB")&&extensionMatches("gb,gbc","GBC"));
    assert(!extensionMatches("gb,gbc","g")&&!validExtensions("gb,crt")&&!validExtensions("gb,")&&!validExtensions(",gb"));

    Manifest manifest{};
    assert(readManifest(launch.root,manifest)&&!strcmp(manifest.id,"HELLO"));
    // The cached table answers for whatever was installed at the last refresh.
    refresh(true);
    assert(associated("Demo.HI")==Associated&&associated("Demo.hi")==Associated);
    assert(associated("Demo.hi.exe")==NotAssociated&&associated("Demo.prg")==NotAssociated);
    assert(associated("noextension")==NotAssociated);

    // A table that cannot speak for /VMS answers Unknown rather than "no", so a
    // caller falls back to the scan instead of dropping the file into stock
    // handling: never scanned, a directory read error, and over the limit.
    refresh(false);assert(associated("Demo.HI")==Unknown);
    failDirError=true;refresh(true);failDirError=false;
    assert(associated("Demo.HI")==Unknown&&extensionCount==0);
    for(unsigned i=0;i<33;i++)fs::create_directories(base/"VMS"/("PAD"+std::to_string(i)));
    refresh(true);assert(associated("Demo.HI")==Unknown&&extensionCount==0);
    for(unsigned i=0;i<33;i++)fs::remove_all(base/"VMS"/("PAD"+std::to_string(i)));
    refresh(true);assert(associated("Demo.HI")==Associated);

    // Path traversal is refused at the component level, before any SD access.
    assert(!absolute("/VMS/../secret",80));assert(!component("../HELLO"));assert(!component("HELLO/VM"));

    // An empty slot refuses before the reboot, and says so rather than booting
    // an image that is not there.
    assert(tryLaunch(rmtSD,"/","HELLO.crt")&&!rebooted);
    assert(message.find("No extension host")!=std::string::npos);

    VmBootImage::install(VM_HOST_SERVICES);
    assert(VmBootImage::installed());
    VmHostId read{};assert(VmBootImage::identity(read)&&read.services==VM_HOST_SERVICES);

    // A host that cannot serve what the module requires is refused here too,
    // and the message names the bits it is missing.
    VmBootImage::install(VM_HOST_SERVICES&~VM_SERVICE_PACKETS);
    assert(tryLaunch(rmtSD,"/","HELLO.crt")&&!rebooted);
    assert(message.find("lacks service")!=std::string::npos);

    // A bit no host here serves is refused by its number, not as a malformed
    // package: the image is well formed and only the host can say otherwise.
    VmBootImage::install(VM_HOST_SERVICES);
    assert(tryLaunch(rmtSD,"/","VENDOR.crt")&&!rebooted);
    assert(message.find("lacks service $10000")!=std::string::npos);

    // A host that speaks another ABI would refuse every module this image can
    // validate, so that is knowable here too.
    VmBootImage::install(VM_HOST_SERVICES,VM_ABI+1);
    assert(tryLaunch(rmtSD,"/","HELLO.crt")&&!rebooted);
    assert(message.find("is ABI")!=std::string::npos);

    // The descriptor's name is third-party bytes on their way to a C64, which executes
    // the control codes rather than drawing them. displayName substitutes; it never
    // drops, so a name that is all control codes still renders something to report.
    {
        char shown[VmBootImage::nameBytes];
        VmHostId probe{};

        // A name filling all twelve bytes with no terminator still comes back whole.
        memcpy(probe.name,"ABCDEFGHIJKL",12);
        VmBootImage::displayName(shown,sizeof shown,&probe);
        assert(!strcmp(shown,"ABCDEFGHIJKL"));

        // $93 clears the screen and $0d ends the line early, taking the "do not power
        // off" warning with it. One visible byte each, and the length is preserved.
        memcpy(probe.name,"AB\x93\x0d""EF\x00\x00\x00\x00\x00\x00",12);
        VmBootImage::displayName(shown,sizeof shown,&probe);
        assert(!strcmp(shown,"AB??EF"));

        // Every byte a control code: twelve substitutes, never an empty row.
        memset(probe.name,0x93,12);
        VmBootImage::displayName(shown,sizeof shown,&probe);
        assert(!strcmp(shown,"????????????")&&strlen(shown)==12);

        // $80-$9f is control as well, and $a0-$ff is not: graphics blocks draw, and so
        // does horizBar $60, which is why the range stops at $7f rather than at ASCII.
        memcpy(probe.name,"\x9b\xa6\xdb\x60\x00\x00\x00\x00\x00\x00\x00\x00",12);
        VmBootImage::displayName(shown,sizeof shown,&probe);
        assert(!strcmp(shown,"?\xa6\xdb\x60"));

        // A name that is empty, or nothing but the two blanks, would reach the screen as
        // no name at all -- during an erase that is the same failure as a cleared one.
        memset(probe.name,0,12);
        VmBootImage::displayName(shown,sizeof shown,&probe);
        assert(!strcmp(shown,"(unnamed)"));
        memset(probe.name,0x20,12);
        VmBootImage::displayName(shown,sizeof shown,&probe);
        assert(!strcmp(shown,"(unnamed)"));
        memset(probe.name,0xa0,12);
        VmBootImage::displayName(shown,sizeof shown,&probe);
        assert(!strcmp(shown,"(unnamed)"));

        // No descriptor at all stays a different answer from a descriptor naming nothing.
        VmBootImage::displayName(shown,sizeof shown,nullptr);
        assert(!strcmp(shown,"(no descriptor)"));

        // A buffer narrower than the source. Every caller in the tree passes nameBytes,
        // so this is the case nothing else reaches -- and it is exactly what happens if
        // a placeholder is added to displayName without widening nameBytes. The comment
        // over nameBytes promises truncation rather than an overrun, by two different
        // bounds: the loop's `n + 1 < bytes` for a real name, snprintf's for the two
        // placeholders. Pinned here so the promise is checked rather than asserted.
        // Canaries either side catch a write that lands outside the buffer at all.
        {
            char fenced[3+4+3];
            const char *const front=fenced, *const back=fenced+3+4;
            char *const narrow=fenced+3;

            memset(fenced,'#',sizeof fenced);
            memcpy(probe.name,"ABCDEFGHIJKL",12);
            VmBootImage::displayName(narrow,4,&probe);
            assert(!strcmp(narrow,"ABC"));                 //3 plus the terminator
            assert(!memcmp(front,"###",3)&&!memcmp(back,"###",3));

            memset(fenced,'#',sizeof fenced);
            memset(probe.name,0,12);                       //drives the (unnamed) branch
            VmBootImage::displayName(narrow,4,&probe);
            assert(strlen(narrow)<4);
            assert(!memcmp(front,"###",3)&&!memcmp(back,"###",3));

            memset(fenced,'#',sizeof fenced);
            VmBootImage::displayName(narrow,4,nullptr);    //and the (no descriptor) one
            assert(strlen(narrow)<4);
            assert(!memcmp(front,"###",3)&&!memcmp(back,"###",3));

            // Zero is the caller having nothing to write into; it must not write anyway.
            memset(fenced,'#',sizeof fenced);
            VmBootImage::displayName(narrow,0,&probe);
            assert(!memcmp(fenced,"##########",sizeof fenced));
        }
    }

    // And the refusals carry the rendered name rather than the field, so nothing the
    // host supplies reaches the screen as a control code.
    VmBootImage::install(VM_HOST_SERVICES,VM_ABI+1,"A\x93""B");
    message.clear();
    assert(tryLaunch(rmtSD,"/","HELLO.crt")&&!rebooted);
    assert(message.find("A?B host is ABI")!=std::string::npos);
    for(unsigned char c:message)assert(c=='\r'||c=='\n'||(c>=0x20&&c<0x80)||c>=0xa0);

    // A host image predating the descriptor cannot say what it provides, and
    // that is not a refusal -- the launch proceeds as it did before. An
    // unserved bit rides through the same way; validation is not the gate.
    VmBootImage::installWithoutDescriptor();
    assert(VmBootImage::installed()&&!VmBootImage::identity(read));
    assert(tryLaunch(rmtSD,"/","HELLO.crt")&&rebooted);
    rebooted=false;message.clear();
    assert(tryLaunch(rmtSD,"/","VENDOR.crt")&&rebooted&&message.empty());

    rebooted=false;marker.clear();
    VmBootImage::install(VM_HOST_SERVICES);

    // Launch by client cartridge: no content file, package identified by descriptor.
    assert(tryLaunch(rmtSD,"/","HELLO.crt"));
    assert(rebooted&&marker=="@VM1");
    Launch saved{};assert(consume(saved)&&!saved.content[0]&&!strcmp(saved.root,"/VMS/HELLO"));

    // Launch by associated content file: the selected path rides along.
    rebooted=false;
    assert(tryLaunch(rmtSD,"/VMS/HELLO/DATA","Sample.hi"));
    assert(rebooted&&consume(saved)&&!strcmp(saved.content,"/VMS/HELLO/DATA/Sample.hi"));

    // Two packages claiming one extension is ambiguous, and ambiguity refuses.
    put(base/"VMS/OTHER/manifest.vmi","VM1\nOTHER\nhi\nengine.mvm\nclient.crt\nEND\n");
    assert(find("hi",nullptr,launch)==-1);
    put(base/"VMS/OTHER/manifest.vmi","VM1\nOTHER\not\nengine.mvm\nclient.crt\nEND\n");
    assert(find("hi",nullptr,launch)==1);
    put(base/"VMS/OTHER/manifest.vmi","VM1\nOTHER\not\n../bad\nclient.crt\nEND\n");
    assert(!readManifest("/VMS/OTHER",manifest));

    // A corrupt module fails preflight, and a failed preflight does not reboot.
    auto module=base/"VMS/HELLO/engine.mvm";
    std::fstream damage(module,std::ios::binary|std::ios::in|std::ios::out);
    damage.seekg(64);const int byte=damage.get();damage.seekp(64);damage.put(byte^1);damage.close();
    assert(!preflight(launch));
    rebooted=false;message.clear();
    assert(tryLaunch(rmtSD,"/","HELLO.crt")&&!rebooted&&!message.empty());

    // A corrupt client cartridge is caught the same way, by its own CRC.
    fs::copy_file(fs::path(argv[1])/"VMS/HELLO/engine.mvm",module,fs::copy_options::overwrite_existing);
    assert(preflight(launch));
    auto client=base/"VMS/HELLO/client.crt";
    std::fstream bank(client,std::ios::binary|std::ios::in|std::ios::out);
    bank.seekg(80);const int first=bank.get();bank.seekp(80);bank.put(first^1);bank.close();
    assert(!preflight(launch));

    puts("PASS: real registry/preflight over packager output; generic extension routing, client and "
         "content launch, one-shot record, ambiguity, traversal, malformed manifest, corrupt module and corrupt client, "
         "extension cache answering Unknown when unscanned, errored or over the limit, "
         "a service belonging to another host refused by its number, and the same module "
         "reaching the reboot when the installed host cannot say what it provides");
}
