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
}
