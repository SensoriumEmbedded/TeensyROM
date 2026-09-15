// SPDX-License-Identifier: MIT
#pragma once
#include "VMRegistry.h"
#include "VMGameCart.h"
#include <new>
namespace VmGameCartLaunch {
static FLASHMEM bool readAt(void *context,uint32_t offset,void *out,uint32_t bytes){
    auto &f=*static_cast<FsFile *>(context);
    return f&&bytes<=INT32_MAX&&f.seekSet(offset)&&f.read(out,bytes)==int32_t(bytes);
}
// Called before registry discovery and ordinary CRT allocation. A recognized
// bundle is always consumed here, including malformed/unsupported versions.
static FLASHMEM bool tryLaunch(uint8_t source,const char *directory,const char *name){
    if(!directory||!directory[0]||!name)return false;
    const bool mpe=VmGameCart::isMpeFile(name);
    if(source!=rmtSD){if(mpe){SendMsgPrintfln("MPE game cartridges require SD");return true;}return false;}
    const char *extension=strrchr(name,'.');if(!mpe&&(!extension||strcasecmp(extension,".crt")))return false;
    char selected[256];const size_t length=strlen(directory);
    const int n=snprintf(selected,sizeof selected,"%s%s%s",directory,length&&directory[length-1]=='/'?"":"/",name);
    if(n<0||n>=int(sizeof selected)||!VmRegistry::absolute(selected,sizeof selected)){SendMsgPrintfln("Invalid or too long cartridge path");return true;}
    FsFile f=SD.sdfs.open(selected,O_RDONLY);char magic[4]{};
    const bool recognized=f&&!f.isDirectory()&&readAt(&f,VmGameCart::DescriptorOffset,magic,4)&&!memcmp(magic,"MGC",3);
    if(!recognized){f.close();if(mpe){SendMsgPrintfln("Invalid or missing MPE game cartridge");return true;}return false;}
    // Keep the two 4 KiB codec buffers off the text menu's bounded stack;
    // release the temporary RAM2 reader before rebooting.
    using CartReader=VmGameCart::Reader;
    void *storage=malloc(sizeof(CartReader));
    if(!storage){f.close();SendMsgPrintfln("Not enough memory to inspect game cartridge");return true;}
    auto *reader=new(storage) CartReader;VmGameCart::Entry module{};VmImageHeader header{};
    const bool valid=f.fileSize()<=VmGameCart::MaxFileBytes&&reader->open(&f,readAt,uint32_t(f.fileSize()))&&
       !(reader->meta().requiredServices&~VM_HOST_SERVICES)&&reader->entry(reader->meta().moduleIndex,module)&&
       reader->readEntry(reader->meta().moduleIndex,0,&header,sizeof header)&&vm_valid_header(header,module.bytes)&&
       header.required_services==reader->meta().requiredServices&&!(header.required_services&~VM_HOST_SERVICES)&&reader->verifyEntry(reader->meta().moduleIndex);
    reader->~CartReader();free(storage);
    f.close();
    if(!valid){SendMsgPrintfln("Invalid or unsupported MPE game cartridge");return true;}
    EEPwriteStr(eepAdCrtBootName,selected);
    char check[256]{};EEPreadNBuf(eepAdCrtBootName,reinterpret_cast<uint8_t *>(check),sizeof check);
    if(!memchr(check,0,sizeof check)||strcmp(check,selected)){SendMsgPrintfln("Cartridge launch path write failed");return true;}
    EEPROM.write(eepAdMinBootInd,MinBootInd_ExecuteMin);
    SetResetAssert;delay(20);REBOOT;return true;
}
}
