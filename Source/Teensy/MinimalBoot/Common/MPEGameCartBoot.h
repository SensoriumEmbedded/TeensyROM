// SPDX-License-Identifier: MIT
#pragma once
#include "VMGameCart.h"

namespace MPEGameCartBoot {
// The GUI-compatible request is the exact physical path in the existing
// EEPROM field. The complete host validates the container again after reset.
// Probe legacy MGC1 .CRT before Ethernet or ordinary cartridge allocation.
static FLASHMEM bool requested(const char *path,bool &sdInitialized){
    if(!path||path[0]!='/'||!memchr(path,0,256)||strstr(path,"..")||strchr(path,'\\')||strchr(path,'*'))return false;
    if(VmGameCart::isMpeFile(path))return true;
    const char *extension=strrchr(path,'.');
    if(!extension||strcasecmp(extension,".crt"))return false;
    sdInitialized=true;
    if(!SD.begin(BUILTIN_SDCARD))return false;
    FsFile file=SD.sdfs.open(path,O_RDONLY);char magic[4]{};
    const bool recognized=file&&!file.isDirectory()&&file.seekSet(VmGameCart::DescriptorOffset)&&
        file.read(magic,sizeof magic)==sizeof magic&&!memcmp(magic,"MGC",3);
    file.close();return recognized;
}
}
