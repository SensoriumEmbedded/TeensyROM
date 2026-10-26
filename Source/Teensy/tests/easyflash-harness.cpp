// SPDX-License-Identifier: MIT
// Production MinimalBoot + IO handlers, with host-only SD/register/allocation
// stand-ins. The separate main profile tests only routing and its static limit.
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define NOGDI
#include <windows.h>
#else
#include <sys/mman.h>
#endif

static std::vector<uint8_t> fixture;
static std::string messages, bootPath;
static unsigned reads, failReadOn, swaps, returnsToMenu, payloadAllocations;
static bool failSeek, failOpen, shortRead, failMetadata;
static unsigned payloadLimit = 2;
static size_t metadataBudget = SIZE_MAX;
static std::map<void*, size_t> allocations;
// Keep the production physical-RAM2 ownership check meaningful even with a
// 32-bit host whose ordinary malloc returns addresses below Teensy's RAM2.
static void* arenaAllocate(size_t bytes) {
  constexpr size_t arenaBytes = 16 * 1024 * 1024;
  static uint8_t* arena = [] {
#ifdef _WIN32
    auto p = VirtualAlloc(reinterpret_cast<void*>(0x20200000), arenaBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    auto p = mmap(reinterpret_cast<void*>(0x20200000), arenaBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
    assert(p == reinterpret_cast<void*>(0x20200000)); return static_cast<uint8_t*>(p);
  }();
  // Every test releases the preceding cartridge before starting another.
  static size_t cursor = 0;
  if (allocations.empty()) cursor = 0;
  bytes = (bytes + 31) & ~size_t(31); assert(bytes <= arenaBytes - cursor);
  void* result = arena + cursor; cursor += bytes; allocations[result] = bytes; return result;
}
struct File {
  size_t pos = 0; bool opened = false;
  File() = default; File(int) {}
  explicit operator bool() const { return opened; }
  size_t position() const { return pos; }
  size_t size() const { return fixture.size(); }
  bool available() const { return opened && pos < fixture.size(); }
  int read() { return available() ? fixture[pos++] : -1; }
  int read(void* destination, size_t length) {
    ++reads;
    if (!opened || pos > fixture.size()) return -1;
    length = std::min(length, fixture.size() - pos);
    if (shortRead || reads == failReadOn) length /= 2;
    memcpy(destination, fixture.data() + pos, length); pos += length; return int(length);
  }
  bool seek(size_t p) { if (failSeek || p > fixture.size()) return false; pos = p; return true; }
  void close() { opened = false; }
};
struct FS { File open(const char*, int) { File file; file.opened = !failOpen; return file; } } SD, USB;
struct SerialMock {
  void printf(const char* format, ...) {
    char text[2048]; va_list args; va_start(args, format); vsnprintf(text, sizeof text, format, args); va_end(args); messages += text;
  }
  void println(const char* text) { messages += text; messages += '\n'; }
  void flush() {}
} Serial;
static uint8_t RAM_Image[16384];
static constexpr size_t RAM_ImageSize = sizeof RAM_Image;
static void* hostMalloc(size_t bytes) {
  if (payloadAllocations >= payloadLimit) return nullptr;
  void* p = arenaAllocate(bytes); ++payloadAllocations; return p;
}
static void* hostCalloc(size_t n, size_t bytes) {
  if (failMetadata || n * bytes > metadataBudget) return nullptr;
  void* p = arenaAllocate(n * bytes); memset(p, 0, n * bytes); return p;
}
static void hostFree(void* p) {
  // Teensy frees RAM2 by its physical address range. On a native host both
  // RAM1's fixed array and RAM2 allocations may be above that address.
  if (!p || (uintptr_t(p) >= uintptr_t(RAM_Image) && uintptr_t(p) < uintptr_t(RAM_Image + sizeof RAM_Image))) return;
  assert(allocations.erase(p) == 1);
}
struct ReturnToMenu {};
struct BootMinimal {};
void runMainTRApp_FromMin() { ++returnsToMenu; throw ReturnToMenu{}; }
static bool game, exrom, led, resetHeld, dmaReleased, doReset;
static uint8_t portData, RegNextIOHndlr, CurrentIOHandler;
static const uint8_t *LOROM_Image, *HIROM_Image;
static uint16_t LOROM_Mask, HIROM_Mask;
static void *fBusSnoop, *PendingfBusSnoop;
static unsigned BigBufCount;
volatile uint8_t EmulateVicCycles;
enum { DMA_S_Idle, DMA_S_StartImmediate, DMA_S_ActiveReady, DMA_S_StartDisable, DMA_S_DisableReady };
static uint8_t DMA_State;
enum { rtUnknown, rtFileCrt, rtBin8kLo, rtBin8kHi, rtBin16k, rtBinC128 };
enum { IOH_None, IOH_EasyFlash, IOH_MagicDesk2, IOH_Num_Handlers };
struct StructMenuItem { const char* Name; size_t Size = 0; uint8_t* Code_Image = nullptr; uint8_t ItemType = rtFileCrt; };
struct stcIOHandlers {
  const char* Name; void(*InitHndlr)(); void(*io1)(uint8_t, bool); void(*io2)(uint8_t, bool);
  void* lo; void* hi; void(*poll)(); void* cycle;
};
static uint8_t DataPortWaitRead() { return portData; }
static void DataPortWriteWaitLog(uint8_t data) { portData = data; }
static void TraceLogAddValidData(uint8_t) {}
static unsigned millis() { return 0; }
static bool VMHostIO2(uint8_t, bool) { return false; }
static void VMHostPoll() {}
#define SetGameAssert (game = true)
#define SetGameDeassert (game = false)
#define SetExROMAssert (exrom = true)
#define SetExROMDeassert (exrom = false)
#define SetLEDOn (led = true)
#define SetLEDOff (led = false)
#define SetResetAssert (resetHeld = true)
#define SetDMADeassert (dmaReleased = true)
#define FILE_READ 0
#define FLASHMEM
#define DMAMEM
#define FeatVMHost
#define Num8kSwapBuffers 2
#define MaxNamePathLength 256
#define MaxPathLength 128
#define Printf_dbg(...) ((void)0)
#define Cart_Generic 0
#define Cart_EasyFlash 32
// Explicit address-model seam: hardware uses 32-bit addresses, native hosts
// may use 64. No production source is regex-rewritten or bank logic replaced.
#define uint32_t uintptr_t
#define SwapSeekAddrMask (uintptr_t{0xef} << (sizeof(uintptr_t) * 8 - 8))
#ifndef TEST_MAIN_IMAGE
#define MinimumBuild
#endif
#include "MinimalBoot/Common/CrtChips.h"
void SendMsgPrintfln(const char*, ...);
uintptr_t toU32(uint8_t*);
uint16_t toU16(uint8_t*);
void FreeCrtChips();
bool SetTypeFromCRT(StructMenuItem*, uint8_t, uint8_t);
bool ParseCRTHeader(StructMenuItem*, uint8_t*, uint8_t*);
static const struct { uint16_t HWID; uint8_t IOH; } HWID_IOH_Assoc[] = {{32, IOH_EasyFlash}, {79, IOH_MagicDesk2}, {85, IOH_MagicDesk2}};
bool AssocHWID_IOH(uint16_t);

#ifdef TEST_MAIN_IMAGE
static uint8_t* ptrRAM_ImageEnd;
uint8_t NumCrtChips;
StructCrtChip CrtChips[MAX_CRT_CHIPS];
static bool LoadedEasyFlashROM;
static StructMenuItem* DriveDirMenu;
static uint8_t IO1[256];
enum { rwRegNextIOHndlr, eepAdNextIOHndlr, eepAdCrtBootName, eepAdMinBootInd, MinBootInd_ExecuteMin };
struct EEPROMMock { uint8_t read(unsigned) { return 42; } void write(unsigned, unsigned) {} } EEPROM;
static uintptr_t flash[0x4000 / sizeof(uintptr_t)];
#define FLASH_BASEADDRESS uintptr_t(flash)
#define UpperAddr 0
static void EEPwriteStr(unsigned, const char* path) { bootPath = path; }
static void RebootTR() { throw BootMinimal{}; }
static void FreeDriveDirMenu() { DriveDirMenu = nullptr; }
static void RedirectEmptyDriveDirMenu() {}
void SendMsgPrintfln(const char* format, ...) { char text[512]; va_list args; va_start(args, format); vsnprintf(text, sizeof text, format, args); va_end(args); Serial.println(text); }
uintptr_t toU32(uint8_t* p) { return uintptr_t(p[0]) << 24 | uintptr_t(p[1]) << 16 | uintptr_t(p[2]) << 8 | p[3]; }
uint16_t toU16(uint8_t* p) { return uint16_t(p[0]) << 8 | p[1]; }
bool AssocHWID_IOH(uint16_t type) { RegNextIOHndlr = type; return type == 32 || type == 79; }
#include "MinimalBoot/Common/C128Crt.h"
// A storage-only seam makes accidental 129th writes observable. The native
// MinimalBoot profile below uses its actual production declarations instead.
static struct { uintptr_t before; StructCrtChip chips[128]; uintptr_t after; } guarded{0xcafebabe, {}, 0xfacefeed};
#define CrtChips guarded.chips
int16_t ParseChipHeader(uint8_t*, const char*);
bool LoadFile(FS*, const char*, StructMenuItem*);
#define malloc hostMalloc
#define free hostFree
#include "main-functions.h"
#undef malloc
#undef free
static StructMenuItem selection{"fixture.crt"};
static bool load(FS* source = &SD) { reads = payloadAllocations = 0; messages.clear(); selection.ItemType = rtFileCrt; return LoadFile(source, "/", &selection); }
static void intact() { assert(guarded.before == 0xcafebabe && guarded.after == 0xfacefeed); }
#else
char DriveDirPath[] = "/";
StructMenuItem DriveDirMenu{"fixture.crt"};
bool PathIsRoot();
bool LoadFile(StructMenuItem*, FS*);
int16_t ParseChipHeader(uint8_t*);
#ifdef VM_HOST_PROFILE
bool ParseLegacyChipHeader(uint8_t*);
#endif
#include "MinimalBoot/Common/IO_Handlers/IOH_EasyFlash.c"
#include "MinimalBoot/Common/IO_Handlers/IOH_MagicDesk2.c"
static stcIOHandlers* IOHandler[] = {nullptr, &IOHndlr_EasyFlash, &IOHndlr_MagicDesk2};
#define malloc hostMalloc
#define calloc hostCalloc
#define free hostFree
#include "MinimalBoot/Min_DriveDirLoad.ino"
#undef malloc
#undef calloc
#undef free
static bool load() { reads = payloadAllocations = 0; messages.clear(); DriveDirMenu.ItemType = rtFileCrt; return LoadFile(&DriveDirMenu, &SD); }
#endif
#undef uint32_t

static void be16(uint8_t* p, unsigned n) { p[0] = n >> 8; p[1] = n; }
static void be32(uint8_t* p, unsigned n) { p[0] = n >> 24; p[1] = n >> 16; p[2] = n >> 8; p[3] = n; }
static void header(unsigned type = 32) { fixture.assign(64, 0); memcpy(fixture.data(), "C64 CARTRIDGE   ", 16); be32(fixture.data()+16, 64); be16(fixture.data()+20, 0x100); be16(fixture.data()+22, type); fixture[24] = type==32?1:0; }
static void chip(unsigned bank, unsigned half, unsigned size = 8192, unsigned type = 2, int fill = -1) {
  const size_t at = fixture.size(); fixture.resize(at + 16 + size); uint8_t* p = fixture.data() + at;
  memcpy(p, "CHIP", 4); be32(p+4, 16+size); be16(p+8, type); be16(p+10, bank); be16(p+12, half ? 0xa000 : 0x8000); be16(p+14, size);
  for (unsigned i = 0; i < size; ++i) p[16+i] = fill < 0 ? bank*11 + half*0x59 + i*7 + (i>>8) : fill;
}
static void image(unsigned banks) { header(); for (unsigned b=0; b<banks; ++b) { chip(b,0); chip(b,1); } }
static void padIgnored(size_t until) {
  while(fixture.size()<until) {
    const size_t remaining=until-fixture.size(); assert(remaining>=16);
    size_t bytes=std::min(size_t(65535),remaining-16);
    if(remaining-16-bytes>0 && remaining-16-bytes<16) bytes-=16;
    chip(65535,0,unsigned(bytes),1);
  }
}
static std::vector<uint8_t> expected(unsigned banks) {
  std::vector<uint8_t> result(banks*16384, 0xff);
  for (size_t at=64; at<fixture.size(); at+=16+toU16(fixture.data()+at+14)) {
    auto p=fixture.data()+at; if (toU16(p+8)!=0 && toU16(p+8)!=2) continue;
    memcpy(result.data()+toU16(p+10)*16384+(toU16(p+12)==0x8000?0:8192), p+16, toU16(p+14));
  }
  return result;
}

#if !defined(TEST_MAIN_IMAGE) && !defined(VM_HOST_PROFILE)
static void select(unsigned value, const std::vector<uint8_t>& bytes, unsigned mask) {
  portData=value; DMA_State=DMA_S_Idle; IO1Hndlr_EasyFlash(0,false);
  if (DMA_State==DMA_S_StartImmediate) { ++swaps; DMA_State=DMA_S_ActiveReady; PollingHndlr_EasyFlash(); assert(DMA_State==DMA_S_StartDisable); }
  assert(CurrentEasyFlashBank==(value&mask));
  assert(!memcmp(LOROM_Image, bytes.data()+(value&mask)*16384,8192));
  assert(!memcmp(HIROM_Image, bytes.data()+(value&mask)*16384+8192,8192));
}
static void rejected(const char* reason) {
  assert(!load()); if(messages.find(reason)==std::string::npos) fprintf(stderr,"Expected diagnostic '%s':\n%s",reason,messages.c_str());
  assert(messages.find(reason)!=std::string::npos); assert(!NumCrtChips && !CrtChips && !EasyFlashErasedBank && !myFile && allocations.empty());
}
static void swapFailure(unsigned half, unsigned failure, bool startup=false) {
  header(); chip(0,0); chip(0,1); chip(1,0); chip(1,1); chip(127,half); assert(load()); InitHndlr_EasyFlash();
  if (startup) { FreeCrtChips(); header(); chip(1,0); chip(1,1); chip(0,0); chip(0,1); assert(load()); }
  else { portData=127; DMA_State=DMA_S_Idle; IO1Hndlr_EasyFlash(0,false); }
  messages.clear(); doReset=false; resetHeld=dmaReleased=false; const unsigned before=returnsToMenu;
  if (failure==0) { myFile.close(); failOpen=true; } else if (failure==1) failSeek=true; else shortRead=true;
  try { if (startup) InitHndlr_EasyFlash(); else { DMA_State=DMA_S_ActiveReady; PollingHndlr_EasyFlash(); } assert(false); }
  catch (ReturnToMenu&) {}
  failOpen=failSeek=shortRead=false;
  assert(returnsToMenu==before+1 && resetHeld && dmaReleased && !doReset && !LOROM_Image && !HIROM_Image && !game && !exrom);
  assert(messages.find(failure==0?"open failed":failure==1?"seek failed":"truncated")!=std::string::npos);
  assert(messages.find("returning to menu")!=std::string::npos); FreeCrtChips();
}
#endif

int main(int argc, char** argv) {
#ifdef TEST_MAIN_IMAGE
  static_assert(MAX_CRT_CHIPS==128); payloadLimit=1024;
  flash[0]=0x42464346; flash[0x1000/sizeof(uintptr_t)]=0x432000d1; flash[0x1000/sizeof(uintptr_t)+1]=uintptr_t(flash)+0x2000;
  for (unsigned bank : {64u,127u,128u,255u}) {
    header(); chip(0,0); chip(bank,1); bootPath.clear();
    try { load(); assert(false); } catch (BootMinimal&) { assert(bootPath=="/fixture.crt"); }
    intact(); FreeCrtChips(); assert(allocations.empty());
    assert(!load(&USB)); assert(messages.find("from SD")!=std::string::npos); intact();
  }
  header(); chip(0,0); chip(0,1); assert(load(&USB) && NumCrtChips==2); intact(); FreeCrtChips();
  header(); chip(65535,0,17,1); chip(0,0); chip(0,1); assert(load(&USB) && NumCrtChips==2); intact(); FreeCrtChips();
  header(79); for (unsigned b=0;b<128;++b) chip(b,0); assert(load(&USB) && NumCrtChips==128); intact(); FreeCrtChips();
  chip(128,0); assert(!load(&USB)); assert(messages.find("128")!=std::string::npos); intact();
  image(64); chip(63,1,8192,2,0xcc); assert(load(&USB) && NumCrtChips==128 && CrtChips[127].ChipROM[0]==0xcc); intact(); FreeCrtChips();
  image(64); chip(64,0); try { load(); assert(false); } catch (BootMinimal&) {} intact(); FreeCrtChips();
  puts("PASS main: static128 canaries, sparse high-bank SD handoff/USB refusal, low-bank USB, ignored types, duplicate reuse and routing before table limit");
#elif defined(VM_HOST_PROFILE)
  static_assert(MAX_CRT_CHIPS==128 && NumDecodeBanks==64);
  for (unsigned b=0;b<64;++b) { BankDecode[b][0]=RAM_Image; BankDecode[b][1]=RAM_Image+8192; }
  for (unsigned value=0;value<256;++value) { portData=value; IO1Hndlr_EasyFlash(0,false); assert(CurrentEasyFlashBank==(value&63) && LOROM_Image==RAM_Image && HIROM_Image==RAM_Image+8192); }
  puts("PASS VM host: original static128 table, 64 decode banks and six-bit register wrapping");
#else
  static_assert(MAX_CRT_CHIPS==512); assert(argc>=3);
  for (unsigned banks : {64u,128u,256u}) {
    image(banks); auto bytes=expected(banks); assert(load() && NumCrtChips==banks*2); InitHndlr_EasyFlash();
    assert(EasyFlashBankMask==banks-1 && EasyFlashErasedBank);
    for (unsigned value=0;value<256;++value) { select(value,bytes,banks-1); select(255-value,bytes,banks-1); }
    for (unsigned value=0;value<256;++value) { portData=value; IO1Hndlr_EasyFlash(2,false); assert(game==bool(value&1)&&exrom==bool(value&2)&&led==bool(value&128)); portData=value^0xa5; IO2Hndlr_EasyFlash(value,false); }
    for (unsigned value=0;value<256;++value) { IO2Hndlr_EasyFlash(value,true); assert(portData==(value^0xa5)); }
    FreeCrtChips(); assert(allocations.empty());
  }
  payloadLimit=0;
  header(); for(unsigned b=1;b<256;++b) if(b%3==0) chip(b,b&1); chip(0,0); chip(0,1); chip(255,1);
  auto bytes=expected(256); assert(load()); InitHndlr_EasyFlash(); for(unsigned b=0;b<256;++b) select(b,bytes,255); FreeCrtChips();
  image(256); chip(255,1,8192,2,0xcc); be16(fixture.data()+fixture.size()-8192-4,0xe000); chip(0,0,8192,0,0x77);
  chip(65535,0,17,1); // ignored packets remain acceptable after all512 unique halves
  bytes=expected(256); assert(load() && NumCrtChips==512); InitHndlr_EasyFlash(); select(255,bytes,255); select(0,bytes,255); FreeCrtChips();
  header(); chip(0,0); chip(0,1); be16(fixture.data()+64+8208+12,0xe000); chip(0,1,8192,0,0x55);
  bytes=expected(64); assert(load() && NumCrtChips==2); InitHndlr_EasyFlash(); select(0,bytes,63); FreeCrtChips();
  header(); for(unsigned n=0;n<600;++n) chip(65535,0,17,1); chip(0,0); chip(0,1); assert(load() && NumCrtChips==2); InitHndlr_EasyFlash(); assert(EasyFlashBankMask==63); FreeCrtChips();
  header(); chip(65535,0,8192,1); rejected("No supported");
  // Duplicate/ignored streams may exceed the cartridge's logical capacity.
  // Both new and replacement swapped payloads must fit the 24-bit file tag.
  for(unsigned overlap : {0u,4096u}) {
    header(); chip(0,0); chip(0,1); chip(1,0);
    padIgnored(0x01000000u-overlap-16u);
    chip(overlap?2:1,0); rejected("24-bit");
  }
  header(); chip(0,0); chip(0,1); padIgnored(0x01000000u-8192u-16u); chip(2,1);
  bytes=expected(64); assert(load()); InitHndlr_EasyFlash(); select(2,bytes,63); FreeCrtChips();
  image(64); be32(fixture.data()+68,0); assert(load()); FreeCrtChips();
  image(64); be32(fixture.data()+68,0xffffffff); assert(load()); FreeCrtChips();
  image(64); be16(fixture.data()+74,256); rejected("bank");
  image(64); be16(fixture.data()+76,0x9000); rejected("address");
  image(64); be16(fixture.data()+78,4096); rejected("4096");
  image(64); fixture[64]='!'; rejected("CHIP");
  image(64); fixture.pop_back(); rejected("Truncated");
  image(64); fixture.resize(fixture.size()-8192-8); rejected("Truncated");
  header(); chip(65535,0,17,1); chip(0,0); chip(0,1); fixture.resize(64+16+16); rejected("Too Short");
  image(64); chip(65535,0,17,1); fixture.pop_back(); rejected("Truncated");
  image(64); failMetadata=true; rejected("RAM"); failMetadata=false;
  image(64); metadataBudget=MAX_CRT_CHIPS*sizeof(StructCrtChip); rejected("erased page"); metadataBudget=SIZE_MAX;
  payloadLimit=2; image(64); failReadOn=7; rejected("Truncated"); failReadOn=0;
  payloadLimit=0;
  for(unsigned half=0;half<2;++half) for(unsigned failure=0;failure<3;++failure) swapFailure(half,failure);
  for(unsigned failure=0;failure<3;++failure) swapFailure(0,failure,true);
  header(79); for(unsigned b=0;b<128;++b) chip(b,0,16384,0); bytes=expected(128);
  assert(load() && NumCrtChips==128 && !EasyFlashErasedBank); InitHndlr_MagicDesk2();
  for(unsigned value=0;value<256;++value) {
    portData=value; DMA_State=DMA_S_Idle; IO1Hndlr_MagicDesk2(0,false);
    if(DMA_State==DMA_S_StartImmediate) { DMA_State=DMA_S_ActiveReady; PollingHndlr_MagicDesk2(); }
    assert(!memcmp(LOROM_Image,bytes.data()+(value&127)*16384,8192));
    assert(!memcmp(HIROM_Image,bytes.data()+(value&127)*16384+8192,8192));
  }
  uint8_t fallback[8192]; const unsigned returned=returnsToMenu; shortRead=true; LoadBank(80,fallback); shortRead=false;
  assert(returnsToMenu==returned && !memcmp(fallback,fixture.data()+80,4096));
  for(unsigned at=4096;at<8192;++at) assert(fallback[at]==0xff);
  FreeCrtChips();
  chip(128,0,16384,0); rejected("128");
  header(0); memcpy(fixture.data(),"C128 CARTRIDGE",14); chip(0,0,16384,0); assert(load()); assert(DriveDirMenu.ItemType==rtBinC128); LaunchC128Cart(); assert(LOROM_Mask==0x3fff); FreeCrtChips();
  for(int index=1;index<argc;++index) {
    std::ifstream input(argv[index],std::ios::binary); assert(input); fixture.assign(std::istreambuf_iterator<char>(input),{});
    unsigned highest=0; for(size_t at=64;at<fixture.size();at+=16+toU16(fixture.data()+at+14)) if(toU16(fixture.data()+at+8)==0||toU16(fixture.data()+at+8)==2) highest=std::max(highest,unsigned(toU16(fixture.data()+at+10)));
    const unsigned banks=highest>=128?256:highest>=64?128:64; bytes=expected(banks); assert(load()); InitHndlr_EasyFlash(); for(unsigned b=0;b<256;++b) select(b,bytes,banks-1); FreeCrtChips();
    printf("PASS cartridge readback: %u banks, complete ROML/ROMH bytes for all256 writes: %s\n",banks,argv[index]);
  }
  assert(allocations.empty() && swaps>1000);
  printf("PASS MinimalBoot: direct production loader/EF/MD2 includes, all256 writes at1/2/4MiB, sparse/out-of-order halves, 512+duplicates, skipped types, diagnostics/menu recovery, allocation failures, legacy128 and C128; %u SD swaps\n",swaps);
#endif
}
