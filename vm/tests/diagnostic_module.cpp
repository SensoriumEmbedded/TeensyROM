// Test-only module: not installed in the NES-only SD kit.
#include "../abi/vm_abi.h"
static const VmHost *host;
static uint16_t cursor;
static bool waiting,frameEnd;
static uint8_t buttons;
static void input(const VmInput *in){buttons=in->buttons;}
static void pump(){}
static bool packet(VmPacket *p){
    if(waiting)return false;*p={};
    if(cursor<1000){
        p->type=1;p->flags=13|(cursor==0?16:0);
        for(unsigned n=0;n<19&&cursor<1000;n++,cursor++){
            auto r=p->payload+n*12;r[0]=cursor;r[1]=cursor>>8;
            for(unsigned y=0;y<8;y++)r[2+y]=((cursor+y)&1)?0x55:0xaa;
            r[10]=0x10|(buttons&15);r[11]=1;p->length+=12;
        }if(cursor==1000)p->flags|=2;
    }else{p->type=2;p->flags=0x25;p->length=26;frameEnd=true;}
    waiting=true;return true;
}
static void ack(){waiting=false;if(frameEnd){frameEnd=false;cursor=0;}}
static const VmModule module={VM_ABI,sizeof(VmModule),input,pump,packet,ack};
extern "C" __attribute__((section(".entry"),used)) const VmModule *vm_entry(const VmHost *h){
    if(!h||h->abi!=VM_ABI||h->bytes<VM_HOST_BASE_BYTES||h->guest_ram_bytes!=VM_RAM_BYTES)return nullptr;
    host=h;
    // Exercise every available working-RAM byte, including both boundaries.
    for(uint32_t i=0;i<h->workspace_bytes;i++)h->workspace[i]=(uint8_t)(i^(i>>8)^0xa5);
    for(uint32_t i=0;i<h->workspace_bytes;i++)if(h->workspace[i]!=(uint8_t)(i^(i>>8)^0xa5))return nullptr;
    for(uint32_t i=0;i<h->guest_ram_bytes;i++)h->guest_ram[i]=(uint8_t)(i^(i>>8)^0x5a);
    for(uint32_t i=0;i<h->guest_ram_bytes;i++)if(h->guest_ram[i]!=(uint8_t)(i^(i>>8)^0x5a))return nullptr;
    VmFileInfo info{};const auto file=h->open(h->package_root,&info);
    if(!file||!info.directory)return nullptr;h->close(file);
    (void)h->micros_now();return &module;
}
