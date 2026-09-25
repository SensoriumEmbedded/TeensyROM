// SPDX-License-Identifier: MIT
#pragma once
#include "Common/VMFiles.h"
#include "Common/VMHostABI.h"
#include "Common/VMFail.h"
//
// The extension image's runtime: reserve the module's memory, load and validate
// the module, and carry packets between it and the C64 client.
//
// The client link is ordinary EasyFlash IO2 register traffic -- the base profile
// never becomes bus master, and nothing in this runtime needs DMA. The loader is
// still built only for --target tr-plus: installing it (DoHostInstall) blanks the
// screen through the full DMA only Fab 0.4 has, and Common_Defs.h stops the compile
// if VM_EXTENSIONS_ENABLED ever reaches a build without Fab04_FullDMACapable.
namespace VmRuntime {
using namespace VmFiles;
static VmRegistry::Launch launch;
static VmRegistry::Manifest manifest;
static VmHostExit host;
static const VmModule *module;
static VmPacket packet;
static uint8_t sequence;
static bool pending;
static volatile bool started;
static volatile bool active, startRequested, inputPending;
static volatile VmInput input;
static uint8_t failure;
static volatile bool quietRequested;
static uint32_t sliceStarted;
static constexpr uint32_t providedServices = VM_HOST_SERVICES;
static void moduleFail(uint8_t error, uint32_t detail);
static void exitToMenu(uint32_t status);
}  // namespace VmRuntime
// Defined in VMBoot.ino, below this include. The sketch preprocessor puts its
// generated prototypes after the includes, so this one has to be written out.
FLASHMEM void RebootToMenu();
namespace VmRuntime {

// Placed by extensionLinkerScript() in tools/lib/extension-image.mjs, and read
// back out of flash by VmBootImage::identity() in the main image.
__attribute__((used, section(".vmhostid")))
const VmHostId vmHostId = { VM_HOSTID_MAGIC, VM_ABI, providedServices,
                            sizeof(VmHostExit), "TeensyROM", 0 };

static void constantAccess(bool protect) {
    // Profile 1 only. Region 13: subregions 2..6 of the aligned 128 KiB RAM2
    // window -- the 80 KiB of constants. Subregion 7 is the reserved top of
    // RAM2 and must stay writable, or the core's fault handler would take a
    // second fault trying to store its crash report.
    // Preserve the core's write-back cache attributes;
    // constants are always XN. RAM2 is unused before module loading, so clean
    // the loader's own writes out of cache before making the span read-only.
    if (protect) arm_dcache_flush_delete((void *)VM_RAM2_RO_BASE, VM_RAM2_RO_BYTES);
    uint32_t mask; __asm__ volatile("mrs %0, primask":"=r"(mask)); __disable_irq();
    __asm__ volatile("dsb":::"memory"); SCB_MPU_CTRL = 0;
    SCB_MPU_RBAR = 0x20260000u | SCB_MPU_RBAR_VALID | 13u;
    SCB_MPU_RASR = protect ? (SCB_MPU_RASR_TEX(1) | SCB_MPU_RASR_C | SCB_MPU_RASR_B |
        SCB_MPU_RASR_AP(7) | SCB_MPU_RASR_XN | SCB_MPU_RASR_SIZE(16) | (0x83u << 8) | SCB_MPU_RASR_ENABLE) : 0;
    SCB_MPU_CTRL = SCB_MPU_CTRL_ENABLE; __asm__ volatile("dsb\nisb":::"memory");
    if (!mask) __enable_irq();
}

static uint32_t timeNow() { return micros(); }
#include "VMHostYield.h"

static bool loadModule() {
    char path[128]; snprintf(path, sizeof path, "%s/%s", launch.root, manifest.module);
    FsFile f = SD.sdfs.open(path, O_RDONLY); VmImageHeader h{};
    if (!f || f.isDirectory() || f.fileSize() > UINT32_MAX || f.read(&h, sizeof h) != sizeof h ||
        !vm_valid_header(h, f.fileSize()) || !vm_host_serves(h, providedServices)) {
        // An image wanting a service this build does not provide is refused
        // here, whole. It is never loaded with the service quietly missing.
        f.close(); failure = 0x11; return false;
    }
    // Bounds, profile and imports are all checked before any module memory is written.
    auto code = (uint8_t *)VM_CODE_BASE;
    auto data = (uint8_t *)VM_DATA_BASE;
    auto ro = (uint8_t *)VM_RAM2_RO_BASE;
    constantAccess(false);
    vm_host_code_window(true);
    const bool loaded = vm_load_payload(h, f, code, data, ro, failure);
    f.close(); vm_host_code_window(false);
    if (!loaded) return false;
    if (h.reserved[0] == VM_PROFILE_RAM2_RO) constantAccess(true);
    __asm__ volatile("dsb\nisb":::"memory");
    const uint32_t used = (h.data_bytes + h.bss_bytes + 31u) & ~31u;
    host = { { VM_ABI, sizeof(VmHostExit), providedServices, data + used, VM_DATA_BYTES - used,
               launch.root, launch.content, timeNow, openFile, readFile, nextFile, closeFile,
               (uint8_t *)VM_RAM_BASE, vm_image_guest_bytes(h), openFlags, writeFile, fileOp,
               shouldYield, moduleFail },
             exitToMenu };
    // Before the call, not after: on profile 0 the record's cache line is
    // inside the arena the module is about to own (VMFail.h).
    VmFail::set(VmFail::Ok);
    module = reinterpret_cast<VmEntry>(h.entry)(&host.base);
    if (!vm_module_table_valid(module, h.code_bytes)) {
        if (!failure) failure = 0x14; module = nullptr; return false;
    }
    return true;
}

#include "VMHostWire.h"
#include "VMHostCommand.h"

static void fail(uint8_t error) {
    failure = error; EZFlashRAM[0xfb] = error;
    __asm__ volatile("dmb":::"memory"); EZFlashRAM[0xf5] = 0xe0;
}
static void moduleFail(uint8_t error, uint32_t detail) {
    EZFlashRAM[0xf8] = detail; EZFlashRAM[0xf9] = detail >> 8; EZFlashRAM[0xfa] = detail >> 16;
    fail(error ? error : 0x16);
}

// Service bit 14. Before this there was no way out of a running module but a
// fault or a hand on the board: this image has no USB, and the module table is
// frozen without a "done" callback. Recording Exited rather than leaving Ok
// standing is the whole point -- Ok already means "handed off, or never came
// back", so a module that finishes cleanly would otherwise be indistinguishable
// from one that hung.
static void exitToMenu(uint32_t status) {
    VmFail::set(VmFail::Exited, status);
    RebootToMenu();   // does not return
    while (true) ;    // the pointer's contract says so even if that ever changes
}
}  // namespace VmRuntime

// Called only by the stock EasyFlash IO2 handler. No file or module code runs
// in an interrupt: this records a request and returns.
bool VMHostIO2(uint8_t address, bool read) {
    using namespace VmRuntime;
    if (!active || CurrentEasyFlashBank != 58) return false;
    if (read) { DataPortWriteWaitLog(EZFlashRAM[address]); return true; }
    const uint8_t value = DataPortWaitRead();
    TraceLogAddValidData(value);
    if (address == 0xf6 || (address >= 0xf8 && address <= 0xfb) || address >= 0xfd) EZFlashRAM[address] = value;
    if (address == 0xf4) { EZFlashRAM[address] = value; commandWrite(value); }
    return true;
}

#include "VMHostPoll.h"

// This image cannot say a word over serial, so every exit below stamps its
// reason into preserved RAM2 for the main image to read. The steps repeat work
// the main image already did in VmRegistry::preflight(), deliberately: the
// machine has reset since, and this side trusts nothing it has not checked.
bool VMHostBoot() {
    using namespace VmRuntime;
    // A cold card in a freshly-entered image is not always ready on the first
    // ask. SDFullInit() in the main image retries for the same reason; there is
    // no mediaPresent() shortcut here, because it cannot tell "no card" from
    // "begin() has not run yet" (docs/Architecture/Known-Issues.md).
    unsigned attempt = 1;
    while (!SD.sdfs.begin(SdioConfig(FIFO_SDIO))) {
        if (++attempt > 3) { VmFail::set(VmFail::SdInit, attempt - 1); return false; }
        delay(50);
    }
    if (!VmRegistry::consume(launch)) { VmFail::set(VmFail::LaunchRecord); return false; }
    if (!VmRegistry::readManifest(launch.root, manifest)) { VmFail::set(VmFail::Manifest); return false; }
    if (manifest.crc != launch.manifest_crc) { VmFail::set(VmFail::ManifestCrc); return false; }
    char path[128]; snprintf(path, sizeof path, "%s/%s", launch.root, manifest.client);
    FsFile f = SD.sdfs.open(path, O_RDONLY); uint8_t header[64], chip[16];
    if (!f) { VmFail::set(VmFail::ClientOpen); return false; }
    if (f.read(header, 64) != 64 || memcmp(header, "C64 CARTRIDGE   ", 16) || header[23] != 32) {
        f.close(); VmFail::set(VmFail::ClientHeader); return false; }
    for (unsigned i = 0; i < 2; i++) {
        if (f.read(chip, 16) != 16 || memcmp(chip, "CHIP", 4) || chip[10] || chip[11] ||
            chip[12] != (i ? 0xa0 : 0x80) || chip[13] || chip[14] != 0x20 || chip[15] ||
            f.read(RAM_Image + i * 8192, 8192) != 8192) { f.close(); VmFail::set(VmFail::ClientBank, i); return false; }
    }
    uint8_t descriptor[128];
    if (f.read(chip, 16) != 16 || memcmp(chip, "CHIP", 4) || chip[10] || chip[11] != 1 || chip[12] != 0x80 || chip[13] ||
        f.read(descriptor, 128) != 128 || memcmp(descriptor, "VMH1", 4) || descriptor[4] != VM_ABI ||
        !memchr(descriptor + 16, 0, 24) || strcmp((char *)descriptor + 16, manifest.id) ||
        vm_crc32(descriptor, 124) != *(uint32_t *)(descriptor + 124)) {
        f.close(); VmFail::set(VmFail::Descriptor); return false; }
    if (vm_crc32(RAM_Image, 16384) != *(uint32_t *)(descriptor + 8)) {
        f.close(); VmFail::set(VmFail::ClientCrc); return false; }
    f.close();
    // Present the client to the C64 as an ordinary 16 KiB EasyFlash cartridge.
    NumCrtChips = 0; memset(EZFlashRAM, 0, sizeof EZFlashRAM); CurrentEasyFlashBank = 0;
    SetGameAssert; SetExROMDeassert;
    for (unsigned i = 0; i < 64; i++) { BankDecode[i][0] = RAM_Image; BankDecode[i][1] = RAM_Image + 8192; }
    LOROM_Image = RAM_Image; HIROM_Image = RAM_Image + 8192;
    LOROM_Mask = HIROM_Mask = 8191;
    CurrentIOHandler = IOH_EasyFlash; EmulateVicCycles = false;
    memcpy(EZFlashRAM + 0xf0, "M3TP", 4); EZFlashRAM[0xf5] = 0;
    active = true;
    // A module failure stays readable by the client rather than hanging -- but
    // it is also recorded, because a client that cannot draw leaves the menu as
    // the only place the reason can surface.
    if (!loadModule()) VmFail::set(VmFail::ModuleLoad, failure);
    doReset = true; return true;
}
