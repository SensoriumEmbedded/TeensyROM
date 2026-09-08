// SPDX-License-Identifier: MIT
#pragma once
#include "VMRegistry.h"
namespace MPELaunch {
static FLASHMEM bool tryFile(uint8_t source, const char *directory, const char *name) {
    if(source!=rmtSD || !directory || !directory[0] || !name ||
       directory[strlen(directory)-1]=='*') return false;
    const char *extension=strrchr(name,'.');
    if(!extension) return false;
    ++extension;
    // No SD registry scan for stock PRG, SID, firmware, disk or text files.
    // CRTs are probed for the VM descriptor; ordinary CRTs fall through.
    if(strcasecmp(extension,"crt") && !VmRegistry::validExtensions(extension)) return false;
    return VmRegistry::tryLaunch(source,directory,name);
}
}
