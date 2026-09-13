#pragma once
#include "VMRegistry.h"
// Generic foreground-only SD services, independently exercised by host tests.
namespace VmFiles {
static FsFile files[24];
static bool fileInfo(FsFile &f,VmFileInfo *out){
    if(f.fileSize()>UINT32_MAX)return false;memset(out,0,sizeof *out);out->bytes=f.fileSize();out->directory=f.isDirectory();
    out->attributes=(f.isReadOnly()?1:0)|(f.isHidden()?2:0)|(f.isDirectory()?16:32);
    f.getModifyDateTime(&out->date,&out->time);
    auto n=f.getName(out->name,sizeof out->name);return n<sizeof out->name-1;
}
static uint32_t openFlags(const char *path,uint32_t flags,VmFileInfo *info){
    if(!VmRegistry::absolute(path,384))return 0;
    if(!(flags&3)||(flags&~31u)||((flags&28)&&!(flags&VM_OPEN_WRITE)))return 0;
    const int mode=(flags&3)==3?O_RDWR:(flags&VM_OPEN_WRITE)?O_WRONLY:O_RDONLY;
    const int extra=((flags&4)?O_CREAT:0)|((flags&8)?O_EXCL:0)|((flags&16)?O_TRUNC:0);
    for(unsigned i=0;i<24;i++)if(!files[i]){files[i]=SD.sdfs.open(path,mode|extra);if(!files[i])return 0;
        if(!fileInfo(files[i],info)){files[i].close();return 0;}return i+1;}return 0;
}
static uint32_t openFile(const char *path,VmFileInfo *info){return openFlags(path,VM_OPEN_READ,info);}
static int32_t readFile(uint32_t h,uint32_t offset,void *p,uint32_t n){
    if(!h||h>24||!files[h-1]||n>INT32_MAX||n>UINT32_MAX-offset||!files[h-1].seekSet(offset))return -1;return files[h-1].read(p,n);
}
static int32_t nextFile(uint32_t h,VmFileInfo *info){
    if(!h||h>24||!files[h-1]||!files[h-1].isDirectory())return -1;
    FsFile entry;while(entry.openNext(&files[h-1],O_RDONLY)){
        bool ok=fileInfo(entry,info);entry.close();if(ok)return 1;
    }return files[h-1].getError()?-1:0;
}
static void closeFile(uint32_t h){if(h&&h<=24)files[h-1].close();}
static int32_t writeFile(uint32_t h,uint32_t offset,const void *p,uint32_t n){
    if(!h||h>24||!files[h-1]||files[h-1].isDirectory()||n>INT32_MAX||n>UINT32_MAX-offset||!files[h-1].seekSet(offset))return -1;
    return files[h-1].write(p,n);
}
static int32_t fileOp(VmFsRequest *r){
    if(!r)return -1;FsFile *f=r->handle&&r->handle<=24?&files[r->handle-1]:nullptr;
    if((uint32_t)r->operation<=(uint32_t)VmFsOp::Close){
        if(!f||!*f)return -1;bool ok=false;
        switch(r->operation){
        case VmFsOp::Flush:ok=f->sync();break;
        case VmFsOp::Truncate:ok=f->truncate(r->value)&&f->sync();break;
        case VmFsOp::Close:ok=f->close();break;
        case VmFsOp::Timestamp:{const uint16_t d=r->value,t=r->extra;ok=f->timestamp(T_WRITE,1980+(d>>9),(d>>5)&15,d&31,t>>11,(t>>5)&63,(t&31)*2)&&f->sync();break;}
        default:break;}return ok?0:-1;
    }
    if(r->operation==VmFsOp::Space){
        const uint32_t c=SD.sdfs.clusterCount(),free=SD.sdfs.freeClusterCount(),s=SD.sdfs.sectorsPerCluster();
        if(!c||free>c||!s)return -1;
        r->value=uint32_t(uint64_t(c)*s>UINT32_MAX?UINT32_MAX:uint64_t(c)*s);
        r->extra=uint32_t(uint64_t(free)*s>UINT32_MAX?UINT32_MAX:uint64_t(free)*s);r->handle=s;return 0;
    }
    if(!VmRegistry::absolute(r->path,384))return -1;
    switch(r->operation){
    case VmFsOp::Mkdir:return SD.sdfs.mkdir(r->path,r->value!=0)?0:-1;
    case VmFsOp::Rmdir:return SD.sdfs.rmdir(r->path)?0:-1;
    case VmFsOp::Remove:return SD.sdfs.remove(r->path)?0:-1;
    case VmFsOp::Rename:return VmRegistry::absolute(r->destination,384)&&SD.sdfs.rename(r->path,r->destination)?0:-1;
    default:return -1;}
}
}
