#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "../../vm/abi/vm_abi.h"

static uint8_t EZFlashRAM[256];
enum {DMA_S_DisableReady,DMA_S_Active};
static unsigned DMA_State;
static uint32_t micros(){return 1234;}
namespace VmRuntime {
static bool active=true,startRequested=false,started=true,inputPending=false,pending=false;
static bool quietRequested=false,packetReplayRequested=false,sliceFail=false;
static uint8_t failure,sequence;
static uint32_t sliceStarted;
static unsigned pumps,acks,slices,replays;
static VmInput input{};
static VmPacket packet{};
struct Camera {void input(uint8_t,uint8_t,uint32_t){}};
struct Video {
  uint16_t geometry=0,capabilities=0;
  uint8_t requested=0,preferred=0,phase=6;
  bool configured=false,hostPacket=false;
  void *nuflix=reinterpret_cast<void *>(1);
  Camera camera;
} indexedVideo;
struct Module {
  void input(const VmInput *){}
  void pump(){++pumps;}
  void ack(){++acks;}
  bool packet(VmPacket *result){*result={};result->type=7;result->length=1;result->payload[0]=0x5a;return true;}
} instance,*module=&instance;
static void fail(uint8_t reason){failure=reason;}
static bool replayPacket(){++replays;return true;}
static void indexedVideoAck(){assert(false);}
static bool transferIndexedVideoSlice(){++slices;return !sliceFail;}
static bool transferIndexedVideo(){assert(false);return false;}
static bool indexedVideoPacket(VmPacket &){return false;}
static void indexedVideoLegacy(){assert(false);}
static unsigned encodePacket(uint8_t *bytes){bytes[0]=packet.type;bytes[1]=packet.payload[0];return 2;}
}
#define VM_INDEXED_SEPARATE_SELECTORS 256
#define VM_INDEXED_CROP_F3 16
#include "audio-host-poll.h"

int main(){
  using namespace VmRuntime;
  VMHostPoll();assert(pumps==1&&slices==1&&pending&&sequence==1&&EZFlashRAM[0]==7);
  const VmPacket frozen=packet;
  VMHostPoll();assert(pumps==2&&slices==2&&pending&&sequence==1&&!std::memcmp(&packet,&frozen,sizeof packet));
  EZFlashRAM[0xf6]=sequence;VMHostPoll();assert(acks==1&&pumps==3&&slices==3&&sequence==2);
  packetReplayRequested=true;VMHostPoll();assert(replays==1&&pumps==3&&slices==3&&quietRequested==false);
  quietRequested=true;VMHostPoll();assert(pumps==3&&slices==3);
  quietRequested=false;sliceFail=true;VMHostPoll();assert(failure==0x17&&slices==4);
  failure=0;pending=false;sliceFail=false;indexedVideo.nuflix=nullptr;
  const auto previousSequence=sequence;VMHostPoll();assert(sequence==previousSequence&&!pending&&slices==5);
  std::puts("PASS: actual transformed host poll publishes SID during NUFLIX upload, progresses DMA behind pending SID, freezes pending bytes, consumes ACK, respects quiet/replay/failure and preserves legacy gating; simulated I/O, no listening acceptance");
}
