// SPDX-License-Identifier: MIT
// Hardware/SD stand-ins around the actual MinimalBoot loader and IO handler.
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <set>
#include <vector>

static std::vector<uint8_t> fixture;
static unsigned blockReads, swaps, readCalls, failReadOn;
static bool failSeek, shortRead;
struct File {
  unsigned pos = 0;
  bool opened = false;
  File() = default;
  File(int) {}
  explicit operator bool() const { return opened; }
  unsigned position() const { return pos; }
  unsigned size() const { return fixture.size(); }
  bool available() const { return opened && pos < fixture.size(); }
  int read() { return available() ? fixture[pos++] : -1; }
  int read(void* destination, unsigned length) {
    ++blockReads;
    ++readCalls;
    if (!opened || pos > fixture.size()) return -1;
    length = std::min(length, unsigned(fixture.size() - pos));
    if (shortRead || readCalls == failReadOn) length /= 2;
    std::memcpy(destination, fixture.data() + pos, length);
    pos += length;
    return length;
  }
  bool seek(unsigned p) {
    if (failSeek || p > fixture.size()) return false;
    pos = p;
    return true;
  }
  void close() { opened = false; }
};
struct FS {
  File open(const char*, int) { File file; file.opened = true; return file; }
} SD;
struct SerialMock { void printf(const char*, ...) {} void flush() {} } Serial;
#define FILE_READ 0
#define MinimumBuild
#define FeatVMHost
#ifdef VM_HOST_PROFILE
#define DMAMEM static_assert(false, "The fixed VM host must not add native cartridge DMAMEM storage");
#else
#define DMAMEM
#endif
#define Num8kSwapBuffers 2
#define MaxNamePathLength 256
#define Printf_dbg(...) ((void)0)
#define Cart_Generic 0
#define Cart_EasyFlash 32
#include "crt-format.h"

static bool game, exrom, led, doReset;
#define SetGameAssert (game = true)
#define SetGameDeassert (game = false)
#define SetExROMAssert (exrom = true)
#define SetExROMDeassert (exrom = false)
#define SetLEDOn (led = true)
#define SetLEDOff (led = false)
enum { DMA_S_Idle, DMA_S_StartImmediate, DMA_S_ActiveReady, DMA_S_StartDisable };
enum { rtUnknown, rtFileCrt, rtBin8kLo, rtBin8kHi, rtBin16k, rtBinC128 };
static uint8_t DMA_State, portData, RegNextIOHndlr;
static const uint8_t *LOROM_Image, *HIROM_Image;
volatile uint8_t EmulateVicCycles;
static bool VMHostIO2(uint8_t, bool) { return false; }
static void VMHostPoll() {}
static uint8_t DataPortWaitRead() { return portData; }
static void DataPortWriteWaitLog(uint8_t data) { portData = data; }
static void TraceLogAddValidData(uint8_t) {}
static unsigned millis() { return 0; }
struct stcIOHandlers {
  const char* name; void(*init)(); void(*io1)(uint8_t, bool); void(*io2)(uint8_t, bool);
  void* lo; void* hi; void(*poll)(); void* cycle;
};
struct StructMenuItem {
  const char* Name;
  uint32_t Size = 0;
  uint8_t* Code_Image = nullptr;
  uint8_t ItemType = rtFileCrt;
};
char DriveDirPath[] = "/";
StructMenuItem DriveDirMenu = { "fixture.crt" };
File myFile;
static uint8_t RAM_Image[16384];
static constexpr unsigned RAM_ImageSize = sizeof(RAM_Image);
static bool PathIsRoot() { return true; }
static void SendMsgPrintfln(const char*, ...) {}
static bool AssocHWID_IOH(uint16_t type) { RegNextIOHndlr = type; return type == 32 || type == 19; }
static uint32_t toU32(uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
static uint16_t toU16(uint8_t* p) { return uint16_t(p[0]) << 8 | p[1]; }
static std::set<void*> ram2;
static bool failMetadata;
static unsigned metadataFailures, metadataAttempts;
static unsigned allocationLimit;
static void (*beforeFree)(void*);
static bool isRAM2(void* p) { return ram2.count(p); }
static void* testMalloc(size_t size) {
  if (ram2.size() >= allocationLimit) return nullptr;
  void* p = std::malloc(size); assert(p); ram2.insert(p); return p;
}
static void* testCalloc(size_t count, size_t size) {
  ++metadataAttempts;
  if (failMetadata) return nullptr;
  if (metadataFailures) { --metadataFailures; return nullptr; }
  return std::calloc(count, size);
}
static void testFree(void* p) { if (beforeFree) beforeFree(p); ram2.erase(p); std::free(p); }
static bool ParseCRTHeader(StructMenuItem*, uint8_t*, uint8_t*);
static bool ParseChipHeader(uint8_t*);
static bool SetTypeFromCRT(StructMenuItem*, uint8_t, uint8_t);
void FreeCrtChips();
#define calloc testCalloc
#include "crt-table.h"
#undef calloc
#define malloc testMalloc
#define free testFree
#include "crt-loader.h"
#undef free
#undef malloc
#include "crt-config.h"
#include "easyflash-handler.h"

// The main image uses RAM2 and falls back to MinimalBoot when it cannot fit.
// Compile its actual parser/loader too: EEPROM and dual-boot flash are hardware
// stand-ins, while allocations, payload reads and cleanup execute unchanged.
#undef MinimumBuild
namespace Full {
struct StructMenuItem {
  const char* Name; uint32_t Size = 0; uint8_t* Code_Image = nullptr; uint8_t ItemType = rtFileCrt;
};
static StructMenuItem menu = { "fixture.crt" };
static StructMenuItem* DriveDirMenu = nullptr;
static uint8_t IO1[256];
enum { rwRegNextIOHndlr, eepAdNextIOHndlr, eepAdCrtBootName, eepAdMinBootInd, MinBootInd_ExecuteMin };
struct EEPROMMock {
  uint8_t read(unsigned) { return 42; }
  void write(unsigned, unsigned) {}
} EEPROM;
static unsigned redirects;
static unsigned directoryReclaims;
static uint32_t fakeFlash[0x1100];
#define FLASH_BASEADDRESS (uintptr_t(fakeFlash))
#define UpperAddr 0
static void EEPwriteStr(unsigned, const char*) { assert(false); }
static void RebootTR() { assert(false); }
static void FreeDriveDirMenu() { ++directoryReclaims; DriveDirMenu = nullptr; }
static void RedirectEmptyDriveDirMenu() { ++redirects; }
bool SetTypeFromCRT(StructMenuItem*, uint8_t, uint8_t);
void FreeCrtChips();
#define calloc testCalloc
#define malloc testMalloc
#define free testFree
#include "full-parser.h"
#include "full-loader.h"
static void *PendingfBusSnoop, *fBusSnoop;
static uint8_t TeensyROMC64_bin[8192], nfcState;
static uint16_t LOROM_Mask, HIROM_Mask;
enum { IOH_None, IOH_EasyFlash, IOH_TeensyROM, nfcStateBitPaused = 1 };
static uint8_t CurrentIOHandler;
#define FLASHMEM
#define SetDMADeassert ((void)0)
#define SetIRQDeassert ((void)0)
#define SetNMIDeassert ((void)0)
#define NVIC_ENABLE_IRQ(...) ((void)0)
#include "full-retire.h"
#include "remote-retire.h"
#undef NVIC_ENABLE_IRQ
#undef SetNMIDeassert
#undef SetIRQDeassert
#undef SetDMADeassert
#undef FLASHMEM
#undef calloc
#undef malloc
#undef free
#undef FLASH_BASEADDRESS
#undef UpperAddr
static bool load() {
  readCalls = 0;
  menu.ItemType = rtFileCrt;
  return LoadFile(&SD, "/", &menu);
}
static unsigned retiredFrees;
static void checkRetired(void*) {
  assert(CurrentIOHandler == IOH_None);
  assert(LOROM_Image == TeensyROMC64_bin && !HIROM_Image);
  ++retiredFrees;
}
static void checkRemoteDetached(void*) {
  assert(CurrentIOHandler == IOH_None && !LOROM_Image && !HIROM_Image);
  assert(!game && !exrom && !PendingfBusSnoop && !fBusSnoop);
  ++retiredFrees;
}
static void checkMenuPreserved(void*) {
  assert(CurrentIOHandler == IOH_TeensyROM && LOROM_Image == TeensyROMC64_bin && !HIROM_Image);
  ++retiredFrees;
}
#define MinimumBuild
}

static void be16(uint8_t* p, unsigned v) { p[0] = v >> 8; p[1] = v; }
static void be32(uint8_t* p, unsigned v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static void header(unsigned type = Cart_EasyFlash) {
  fixture.assign(64, 0);
  std::memcpy(fixture.data(), "C64 CARTRIDGE   ", 16);
  be32(fixture.data() + 16, 64); be16(fixture.data() + 20, 0x100);
  be16(fixture.data() + 22, type); fixture[24] = 1;
}
static void chip(unsigned bank, unsigned half, unsigned size = 8192, unsigned type = 2) {
  const unsigned offset = fixture.size(); fixture.resize(offset + 16 + size);
  uint8_t* p = fixture.data() + offset;
  std::memcpy(p, "CHIP", 4); be32(p + 4, 16 + size); be16(p + 8, type);
  be16(p + 10, bank); be16(p + 12, half ? 0xa000 : 0x8000); be16(p + 14, size);
  for (unsigned i = 0; i < size; ++i) p[16 + i] = bank * 11 + half * 0x59 + i * 7 + (i >> 8);
}
static void image(unsigned banks) {
  header();
  for (unsigned b = 0; b < banks; ++b) { chip(b, 0); chip(b, 1); }
}
static bool load() { readCalls = 0; DriveDirMenu.ItemType = rtFileCrt; return LoadFile(&DriveDirMenu, &SD); }
static std::vector<uint8_t> expected(unsigned banks) {
  std::vector<uint8_t> bytes(banks * 16384, 0xff);
  for (unsigned offset = 64; offset < fixture.size(); offset += 16 + toU16(fixture.data() + offset + 14)) {
    uint8_t* p = fixture.data() + offset;
    const unsigned bank = toU16(p + 10), half = toU16(p + 12) == 0x8000 ? 0 : 1;
    std::memcpy(bytes.data() + bank * 16384 + half * 8192, p + 16, toU16(p + 14));
  }
  return bytes;
}
static void select(unsigned data, const std::vector<uint8_t>& bytes, unsigned mask) {
  portData = data; DMA_State = DMA_S_Idle; IO1Hndlr_EasyFlash(0, false);
  if (DMA_State == DMA_S_StartImmediate) {
    ++swaps; DMA_State = DMA_S_ActiveReady; PollingHndlr_EasyFlash();
    assert(DMA_State == DMA_S_StartDisable);
  }
  const unsigned bank = data & mask;
  assert(CurrentEasyFlashBank == bank);
  assert(LOROM_Image && HIROM_Image);
  assert(!std::memcmp(LOROM_Image, bytes.data() + bank * 16384, 8192));
  assert(!std::memcmp(HIROM_Image, bytes.data() + bank * 16384 + 8192, 8192));
}
static void rejected(bool full = false) {
  if (full) {
    const unsigned before = Full::redirects;
    assert(!Full::load()); assert(Full::NumCrtChips == 0); assert(Full::CrtChips == nullptr);
    assert(Full::redirects > before && Full::IO1[Full::rwRegNextIOHndlr] == 42);
  } else {
    assert(!load()); assert(NumCrtChips == 0); assert(CrtChips == nullptr); assert(!myFile);
  }
  assert(ram2.empty());
}
static void malformed(bool full) {
  image(128); be16(fixture.data() + 64 + 10, 256); rejected(full);
  image(128); fixture.pop_back(); rejected(full);
  image(128); fixture.resize(fixture.size() - 8192 - 8); rejected(full);
  image(128); be16(fixture.data() + 64 + 2 * (16 + 8192) + 10, 0); rejected(full);
  image(64); // Duplicate ROMH through its alternate address must be rejected.
  be16(fixture.data() + 64 + 3 * (16 + 8192) + 10, 0);
  be16(fixture.data() + 64 + 3 * (16 + 8192) + 12, 0xe000); rejected(full);
  image(256); chip(0, 0); rejected(full); // 513th packet, with no counter wrap
  image(64); be32(fixture.data() + 64 + 4, 16 + 8191); rejected(full);
  image(64); be16(fixture.data() + 64 + 8, 1); rejected(full);
  image(64); be16(fixture.data() + 64 + 12, 0x9000); rejected(full);
  image(64); be16(fixture.data() + 64 + 14, 4096); rejected(full);
  image(64); fixture[64] = '!'; rejected(full);
}
int main(int argc, char** argv) {
#ifdef VM_HOST_PROFILE
  static_assert(NumDecodeBanks == 64, "The VM carrier retains its fixed 64-bank decode table");
  // VMHost initializes a validated 16 KiB carrier directly and uses all six
  // ordinary EasyFlash register bits. This profile must add no native CRT pool.
  for (unsigned bank = 0; bank < 64; ++bank) {
    BankDecode[bank][0] = RAM_Image; BankDecode[bank][1] = RAM_Image + 8192;
  }
  for (unsigned value = 0; value < 256; ++value) {
    portData = value; DMA_State = DMA_S_Idle; IO1Hndlr_EasyFlash(0, false);
    assert(CurrentEasyFlashBank == (value & 63));
    assert(LOROM_Image == RAM_Image && HIROM_Image == RAM_Image + 8192 && DMA_State == DMA_S_Idle);
  }
  std::puts("PASS EasyFlash VM host profile: fixed decode allocation, no extra DMAMEM pool, all 256 register writes");
#else
  // Two RAM1 halves, a little RAM2, and repeated SD refills. Every $DE00 value
  // is exercised at every capacity; the 4 MiB fixture has all 512 packets.
  allocationLimit = 2;
  for (unsigned banks : {64u, 128u, 256u}) {
    image(banks); auto bytes = expected(banks); assert(load());
    assert(NumCrtChips == banks * 2); InitHndlr_EasyFlash();
    assert(EasyFlashBankMask == banks - 1); assert(game && !exrom && !EmulateVicCycles);
    for (unsigned data = 0; data < 256; ++data) select(data, bytes, banks - 1);
    for (unsigned data = 0; data < 256; ++data) select(255 - data, bytes, banks - 1);
    for (unsigned data = 0; data < 256; ++data) {
      portData = data; IO1Hndlr_EasyFlash(2, false);
      assert(game == bool(data & 1) && exrom == bool(data & 2) && led == bool(data & 0x80));
      portData = data ^ 0xa5; IO2Hndlr_EasyFlash(data, false);
    }
    for (unsigned address = 0; address < 256; ++address) {
      portData = 0; IO2Hndlr_EasyFlash(address, true); assert(portData == (address ^ 0xa5));
    }
  }
  assert(swaps > 1000 && blockReads > swaps);

  // Sparse ROML/ROMH, including an absent startup ROMH and the top bank alone.
  allocationLimit = 0;
  header(); chip(0, 0); chip(255, 1); auto sparse = expected(256);
  assert(load()); InitHndlr_EasyFlash();
  for (unsigned data = 0; data < 256; ++data) select(data, sparse, 255);
  header(); chip(0, 1); chip(127, 0); sparse = expected(128);
  assert(load()); InitHndlr_EasyFlash();
  for (unsigned data = 0; data < 256; ++data) select(data, sparse, 127);
  // $E000 packets address the same ROMH half as $A000 in Ultimax mode.
  header(); chip(0, 0, 8192, 0); chip(127, 1, 8192, 0);
  be16(fixture.data() + 64 + 16 + 8192 + 12, 0xe000); sparse = expected(128);
  assert(load()); InitHndlr_EasyFlash(); select(255, sparse, 127);
  // CHIP order need not put the startup bank in RAM. Both halves must be
  // resolved before Init examines the EAPI signature or the C64 starts.
  header();
  for (unsigned b = 1; b < 128; ++b) { chip(b, 0); chip(b, 1); }
  chip(0, 0); chip(0, 1); auto reordered = expected(128);
  assert(load()); InitHndlr_EasyFlash();
  assert(!std::memcmp(LOROM_Image, reordered.data(), 8192));
  assert(!std::memcmp(HIROM_Image, reordered.data() + 8192, 8192));
  select(127, reordered, 127); select(0, reordered, 127);

  // Failed SD refills detach the ROM instead of publishing a partial buffer
  // or returning a tagged virtual pointer to the C64.
  for (unsigned failure = 0; failure < 2; ++failure) {
    image(128); assert(load()); InitHndlr_EasyFlash(); doReset = false;
    portData = 127; DMA_State = DMA_S_Idle; IO1Hndlr_EasyFlash(0, false);
    assert(DMA_State == DMA_S_StartImmediate);
    failSeek = failure == 0; shortRead = failure == 1;
    DMA_State = DMA_S_ActiveReady; PollingHndlr_EasyFlash();
    assert(DMA_State == DMA_S_StartDisable && doReset && !game && !exrom);
    assert(!LOROM_Image && !HIROM_Image);
    failSeek = shortRead = false;
  }

  malformed(false);
  image(64); fixture[0] = '!'; assert(!load());
  image(64); be32(fixture.data() + 16, 63); assert(!load());
  FreeCrtChips();
  image(128); failSeek = true; rejected(); failSeek = false;
  image(128); shortRead = true; rejected(); shortRead = false;
  image(128); failMetadata = true; rejected(); failMetadata = false;
  image(128); assert(load()); assert(NumCrtChips == 256); FreeCrtChips();
  allocationLimit = 2; image(64); failReadOn = 7; rejected(); failReadOn = 0;

  // The EasyFlash-only rules must not reject existing generic 16 KiB CHIP
  // packets or apply an eight-bit bank mask after loading another cartridge.
  header(Cart_Generic); fixture[24] = 0; chip(0, 0, 16384, 0); assert(load());
  assert(NumCrtChips == 1 && !LoadedEasyFlashROM && DriveDirMenu.ItemType == rtBin16k);
  assert(CrtChips[0].ROMSize == 16384); FreeCrtChips();
  assert(argc == 2);
  std::ifstream bundled(argv[1], std::ios::binary); assert(bundled);
  fixture.assign(std::istreambuf_iterator<char>(bundled), std::istreambuf_iterator<char>());
  auto bundledBytes = expected(64); assert(load()); InitHndlr_EasyFlash();
  assert(EasyFlashBankMask == 63);
  for (unsigned data = 0; data < 256; ++data) select(data, bundledBytes, 63);
  FreeCrtChips();

  allocationLimit = 1024;
  image(128); Full::DriveDirMenu = &Full::menu; metadataFailures = 1;
  const unsigned attemptsBefore = metadataAttempts, reclaimsBefore = Full::directoryReclaims;
  assert(Full::load());
  assert(metadataAttempts == attemptsBefore + 2 && Full::directoryReclaims == reclaimsBefore + 1);
  assert(!Full::DriveDirMenu && Full::NumCrtChips == 256); Full::FreeCrtChips();
  image(128); Full::DriveDirMenu = &Full::menu; failMetadata = true;
  const unsigned failedAttempts = metadataAttempts, failedReclaims = Full::directoryReclaims;
  rejected(true); failMetadata = false;
  assert(metadataAttempts == failedAttempts + 2 && Full::directoryReclaims == failedReclaims + 1);
  for (unsigned banks : {64u, 128u, 256u}) {
    image(banks); auto bytes = expected(banks); assert(Full::load());
    assert(Full::LoadedEasyFlashROM && Full::NumCrtChips == banks * 2);
    for (unsigned packet = 0; packet < Full::NumCrtChips; ++packet) {
      auto& current = Full::CrtChips[packet];
      assert(current.BankNum == packet / 2 && current.ROMSize == 8192);
      assert(!std::memcmp(current.ChipROM, bytes.data() + packet * 8192, 8192));
    }
    Full::FreeCrtChips(); assert(ram2.empty());
  }
  malformed(true);
  image(64); fixture[0] = '!'; assert(!Full::load());
  image(64); be32(fixture.data() + 16, 63); assert(!Full::load());
  image(64); shortRead = true; assert(!Full::load()); shortRead = false;
  image(128); failMetadata = true; rejected(true); failMetadata = false;
  image(128); failReadOn = 7; rejected(true); failReadOn = 0;
  allocationLimit = 0; image(64); rejected(true); // RAM2 exhaustion/MinimalBoot handoff guard
  allocationLimit = 1024;
  header(Cart_Generic); fixture[24] = 0; chip(0, 0, 16384, 0); assert(Full::load());
  assert(!Full::LoadedEasyFlashROM && Full::menu.ItemType == rtBin16k);
  Full::FreeCrtChips();
  header(19);
  for (unsigned bank = 0; bank < 128; ++bank) chip(bank, 0, 8192, 0);
  assert(Full::load() && Full::NumCrtChips == 128); Full::FreeCrtChips();
  chip(128, 0, 8192, 0); rejected(true); // Other handlers keep their 128-packet limit.
  image(64); assert(Full::load());
  Full::CurrentIOHandler = Full::IOH_EasyFlash;
  LOROM_Image = Full::CrtChips[2].ChipROM; HIROM_Image = Full::CrtChips[3].ChipROM;
  beforeFree = Full::checkRetired;
  Full::SetUpMainMenuROM();
  beforeFree = nullptr;
  assert(Full::retiredFrees > 0 && Full::CrtChips == nullptr && Full::NumCrtChips == 0);
  image(64); assert(Full::load());
  Full::CurrentIOHandler = Full::IOH_EasyFlash;
  Full::PendingfBusSnoop = Full::fBusSnoop = RAM_Image;
  LOROM_Image = Full::CrtChips[2].ChipROM; HIROM_Image = Full::CrtChips[3].ChipROM;
  beforeFree = Full::checkRemoteDetached; Full::RetireRemoteCartridge(); beforeFree = nullptr;
  image(64); assert(Full::load());
  Full::CurrentIOHandler = Full::IOH_TeensyROM;
  LOROM_Image = Full::TeensyROMC64_bin; HIROM_Image = nullptr;
  beforeFree = Full::checkMenuPreserved; Full::RetireRemoteCartridge(); beforeFree = nullptr;
  assert(ram2.empty());
  std::printf("PASS EasyFlash firmware regressions: full and MinimalBoot loaders/parsers, 1/2/4 MiB, 512 CHIP packets, all 256 bank writes, ROML/ROMH bytes, sparse erased halves, %u SD swaps, controls/IO2, malformed input, heap reclaim/retry, handler retirement and bundled Robocop compatibility\n", swaps);
#endif
}
