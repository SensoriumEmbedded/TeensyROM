// SPDX-License-Identifier: MIT
#pragma once
#include "VMRegistry.h"
namespace VmLaunch {
static FLASHMEM bool tryFile(uint8_t source, const char *directory, const char *name) {
    if(source!=rmtSD || !directory || !directory[0] || !name ||
       directory[strlen(directory)-1]=='*') return false;
    const char *extension=strrchr(name,'.');
    if(!extension) return false;
    ++extension;
    // No SD registry scan for stock PRG, SID, firmware, disk or text files.
    // CRTs are probed for the VM descriptor; ordinary CRTs fall through, and
    // no manifest may claim crt, so the cached table has nothing to say there.
    if(strcasecmp(extension,"crt")){
        if(!VmRegistry::validExtensions(extension)) return false;
        if(VmRegistry::associated(name)==VmRegistry::NotAssociated) return false;
    }
    return VmRegistry::tryLaunch(source,directory,name);
}
// The selected .sid, to the SIDHost extension instead of the built-in player (the menu's
// Shift+S). The same SD-only rule as tryFile: false when the selection is not a .sid on
// the SD card, so the caller can say so; true once it is handled, by a reboot or a refusal.
// badFile, when given, says whether a refusal was of the file itself, which the built-in
// player would refuse too.
static FLASHMEM bool launchInHost(uint8_t source, const char *directory, const char *name, bool *badFile=nullptr) {
    if(source!=rmtSD || !directory || !directory[0] || !name ||
       directory[strlen(directory)-1]=='*') return false;
    const char *extension=strrchr(name,'.');
    if(!extension || strcasecmp(extension+1,"sid")) return false;
    const bool fileOk=VmRegistry::launchInHost(directory,name);
    if(badFile)*badFile=!fileOk;
    return true;
}
// A .crt that is an extension package rather than a cartridge: the test tryLaunch makes, without
// launching it or validating it any further.
static FLASHMEM bool isPackageCrt(uint8_t source, const char *directory, const char *name) {
    if(source!=rmtSD || !directory || !directory[0] || !name) return false;
    const char *extension=strrchr(name,'.');
    if(!extension || strcasecmp(extension+1,"crt")) return false;
    char path[256];
    if(snprintf(path,sizeof path,"%s%s%s",directory,directory[strlen(directory)-1]=='/'?"":"/",name)>=(int)sizeof path)
        return false;
    uint8_t d[128];
    return VmRegistry::clientDescriptor(path,d);
}
}
