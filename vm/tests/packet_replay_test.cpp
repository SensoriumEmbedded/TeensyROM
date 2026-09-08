#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "../abi/vm_abi.h"
#include "../video/mpe_video_camera.h"
#define FeatVMVideoDMA
#define Fab04_FullDMACapable
static uint8_t EZFlashRAM[256],c64[65536];
static unsigned DMA_State,nS_DMASetup,nS_MaxAdj,transfers,failAt,emergency;
static constexpr unsigned DMA_S_DisableReady=0,Def_nS_DMASetupNTSC=1,Def_nS_DMASetupPAL=2,Def_nS_MaxAdjNTSC=3,Def_nS_MaxAdjPAL=4;
static bool PerformDMA(bool,uint16_t a,uint8_t *p,uint16_t n,bool){
    ++transfers;DMA_State=1;if(failAt==1)return false;memcpy(c64+a,p,n);return true;
}
static bool AGIContinueDMA(bool,uint16_t a,uint8_t *p,uint16_t n,bool){
    if(failAt==2)return false;memcpy(c64+a,p,n);return true;
}
static bool CloseDMA(){if(failAt==3)return false;DMA_State=0;return true;}
static void AGIDMAEmergencyRelease(){++emergency;DMA_State=0;}
static uint32_t micros(){return 0;}
namespace VmRuntime {
static const VmModule *module;static VmPacket packet;
static bool active=true,started=true,startRequested,inputPending,pending,quietRequested,packetReplayRequested;
static uint8_t failure,sequence,videoTiming=0x83;static uint32_t sliceStarted;static VmInput input;
static struct {bool configured,hostPacket;uint8_t phase,preferred,capabilities,requested;uint16_t geometry;mpe_video::CropCamera camera;} indexedVideo{};
static unsigned pumps,acks,offers;static bool offer=true;
static void fail(uint8_t e){failure=e;}
static uint16_t crc16(const uint8_t *p,unsigned n){uint16_t c=65535;while(n--){c^=uint16_t(*p++)<<8;for(unsigned b=0;b<8;b++)c=(c<<1)^((c&0x8000)?0x1021:0);}return c;}
#include "../../Source/Teensy/MinimalBoot/VMPacketReplay.h"
static bool transferIndexedVideo(){assert(false);return false;}
static bool transferIndexedVideoSlice(){assert(false);return false;}
static void indexedVideoAck(){assert(false);}
static void indexedVideoLegacy(){}
static bool indexedVideoPacket(VmPacket &){return false;}
}
#include "../../Source/Teensy/MinimalBoot/VMHostPoll.h"
int main(){using namespace VmRuntime;
    static const VmModule spy{VM_ABI,sizeof(VmModule),[](const VmInput *){},[](){pumps++;},[](VmPacket *p){
        if(!offer)return false;offer=false;offers++;*p={};p->type=2;p->length=26;
        for(unsigned i=0;i<26;i++)p->payload[i]=i*7;return true;
    },[](){acks++;}};module=&spy;
    for(unsigned length:{26u,228u}){
        failure=sequence=0;pending=quietRequested=packetReplayRequested=false;offer=true;EZFlashRAM[0xf6]=0;
        VMHostPoll();assert(pending&&sequence==1&&!transfers);packet.length=length;
        uint8_t frozen[240];const auto size=encodePacket(frozen);const auto oldPacket=packet;
        memset(c64,0x6c,sizeof c64);memset(EZFlashRAM,0xb7,0xf0);
        const auto oldPumps=pumps,oldAcks=acks,oldOffers=offers;
        packetReplayRequested=quietRequested=true;DMA_State=1;VMHostPoll();assert(packetReplayRequested&&!transfers);
        DMA_State=0;VMHostPoll();assert(!packetReplayRequested&&pending&&quietRequested);
        assert(pumps==oldPumps&&acks==oldAcks&&offers==oldOffers&&sequence==1);
        assert(!memcmp(c64+0x2800,frozen,size)&&c64[0x02f0]==0xa5);
        for(unsigned n=0;n<sizeof c64;n++)if(n!=0x2f0&&(n<0x2800||n>=0x2800+size))assert(c64[n]==0x6c);
        assert(!memcmp(&packet,&oldPacket,sizeof packet)&&EZFlashRAM[0xf5]==0x12);
        VMHostPoll();assert(pumps==oldPumps&&acks==oldAcks);
        EZFlashRAM[0xf6]=1;VMHostPoll();assert(!pending&&!quietRequested&&acks==oldAcks+1&&pumps==oldPumps+1);
        transfers=0;
    }
    for(failAt=1;failAt<=3;failAt++){
        failure=0;pending=packetReplayRequested=quietRequested=true;EZFlashRAM[0xf6]=0;const auto oldAcks=acks;
        VMHostPoll();assert(failure==0x19&&pending&&acks==oldAcks&&DMA_State==0);
    }
    assert(emergency==2);
    puts("PASS actual VM host replay: frozen SID/max packet, RAM guards, quiet/ACK order, busy DMA and all DMA failures");
}
