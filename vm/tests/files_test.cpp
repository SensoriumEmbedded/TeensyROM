#include "fake_sd.h"
#include "../../Source/Teensy/MinimalBoot/Common/VMFiles.h"
int main(int argc,char **argv){
 assert(argc==2);base=argv[1];assert(base.string().find("files-sandbox-")!=std::string::npos);fs::create_directories(base/"VMS/DOSVM/D");
 using namespace VmFiles;VmFileInfo i{};VmFsRequest r{};
 assert(!openFlags("/../escape",7,&i));assert(!openFlags("relative",7,&i));assert(!openFlags("/VMS/DOSVM/D/X.TXT",VM_OPEN_CREATE,&i));
 auto h=openFlags("/VMS/DOSVM/D/X.TXT",VM_OPEN_READ|VM_OPEN_WRITE|VM_OPEN_CREATE|VM_OPEN_EXCLUSIVE,&i);assert(h);
 assert(writeFile(h,0,"abc",3)==3);r={VmFsOp::Flush,h};assert(fileOp(&r)==0);
 char data[4]{};assert(readFile(h,0,data,3)==3&&!strcmp(data,"abc"));
 assert(readFile(0,0,data,1)<0&&writeFile(25,0,data,1)<0&&writeFile(h,UINT32_MAX,data,2)<0);
 failWrite=true;assert(writeFile(h,0,"z",1)!=1);failWrite=false;
 failFlush=true;r={VmFsOp::Flush,h};assert(fileOp(&r)<0);failFlush=false;
 r={VmFsOp::Truncate,h,2};assert(fileOp(&r)==0);r={VmFsOp::Close,h};assert(fileOp(&r)==0);
 assert(!openFlags("/VMS/DOSVM/D/X.TXT",VM_OPEN_WRITE|VM_OPEN_CREATE|VM_OPEN_EXCLUSIVE,&i));
 h=openFile("/VMS/DOSVM/D/X.TXT",&i);assert(h&&i.bytes==2&&i.attributes==32&&i.date==33);
 assert(writeFile(h,0,"z",1)!=1);closeFile(h);
 r={VmFsOp::Rename,0,0,0,"/VMS/DOSVM/D/X.TXT","/VMS/DOSVM/D/Y.TXT"};assert(fileOp(&r)==0);
 r={VmFsOp::Rename,0,0,0,"/VMS/DOSVM/D/Y.TXT","/../escape"};assert(fileOp(&r)<0);
 h=openFile("/VMS/DOSVM/D",&i);assert(h&&i.directory);assert(nextFile(h,&i)==1&&!strcmp(i.name,"Y.TXT"));assert(nextFile(h,&i)==0);closeFile(h);
 uint32_t handles[24];for(auto &handle:handles){handle=openFile("/VMS/DOSVM/D/Y.TXT",&i);assert(handle);}assert(!openFile("/VMS/DOSVM/D/Y.TXT",&i));for(auto handle:handles)closeFile(handle);
 r={VmFsOp::Space};assert(fileOp(&r)==0&&r.value==8192&&r.extra==4096&&r.handle==8);
 r={VmFsOp::Mkdir,0,0,0,"/VMS/DOSVM/D/EMPTY",nullptr};assert(fileOp(&r)==0);
 r.operation=VmFsOp::Rmdir;assert(fileOp(&r)==0);
 r={VmFsOp::Remove,0,0,0,"/VMS/DOSVM/D/Y.TXT",nullptr};assert(fileOp(&r)==0&&!fs::exists(base/"VMS/DOSVM/D/Y.TXT"));
 puts("PASS: actual generic host file services; read/write/flush/truncate/rename/list/space, 24 handles, invalid paths/flags/spans and injected write/flush failures");
}
