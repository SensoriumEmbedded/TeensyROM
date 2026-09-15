// SPDX-License-Identifier: MIT
#pragma once
#include "VMRegistry.h"
#include "VMGameCartLaunch.h"
namespace MPELaunch {
static FLASHMEM bool tryFile(uint8_t source, const char *directory, const char *name) {
    if(!directory || !directory[0] || !name) return false;
    if(directory[strlen(directory)-1]=='*') {
        if(VmGameCart::isMpeFile(name)){SendMsgPrintfln("MPE game cartridges require a physical SD file");return true;}
        return false;
    }
    if(VmGameCartLaunch::tryLaunch(source,directory,name)) return true;
    if(source!=rmtSD) return false;
    const char *extension=strrchr(name,'.');
    if(!extension) return false;
    ++extension;
    // No SD registry scan for stock PRG, SID, firmware, disk or text files.
    // CRTs are probed for the VM descriptor; ordinary CRTs fall through.
    if(strcasecmp(extension,"crt") && !VmRegistry::validExtensions(extension)) return false;
    return VmRegistry::tryLaunch(source,directory,name);
}
}
