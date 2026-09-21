// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include <stddef.h>

// TeensyROM extension ABI, version 2 -- BASE PROFILE.
//
// An extension is a natively-compiled ARMv7E-M hard-float module, loaded from
// the SD card into a reserved ITCM/DTCM window and called by a dedicated boot
// image. Modules are trusted local code, not a security sandbox: every pointer
// and callback handed over here lives until reset, and none may be called from
// an interrupt.
//
// This header is the whole contract. Firmware and modules compile the same
// file; nothing else is shared between them, and a module built against this
// header needs no TeensyROM source to build.
//
// WHY "BASE PROFILE"
// The loader deliberately implements less than the ABI can express. VmHost
// grows only at its tail, and host->bytes says how far this host's copy runs,
// so a later host can append callbacks without renumbering or recompiling
// anything below. A module that wants more than the base offers asks for it in
// its image header (required_services); a host that does not provide a
// requested bit refuses the image outright rather than half-loading it. That
// refusal is the negotiation: build the capability behind a service bit, retry
// without it, and one module binary runs on both hosts.
//
// Service bits 0..4 below are the base profile; bit 7 is this loader's
// optional RAM2 memory profile. Neither set will change meaning. The bits
// listed as reserved are assigned to known out-of-tree extensions so the two
// sides cannot collide; the loader rejects them, but no future loader release
// will reuse the numbers for something else.
enum : uint32_t { VM_ABI = 2, VM_CODE_BASE = 0x18000, VM_CODE_LIMIT = 0x30000,
                  VM_DATA_BASE = 0x20014000, VM_DATA_LIMIT = 0x20044000,
                  VM_DATA_BYTES = VM_DATA_LIMIT-VM_DATA_BASE,
                  VM_RAM_BASE = 0x20200000,
                  VM_RAM_BYTES = 512*1024,
                  VM_RAM_LIMIT = VM_RAM_BASE+VM_RAM_BYTES,
                  // Profile 1 only: the MPU subregion size for this window,
                  // so the smallest span it can hold back. It holds back the
                  // top one because the core keeps its CrashReport there and
                  // write-protecting it would fault the core's crash writer.
                  VM_RAM_RESERVED_BYTES = 16*1024 };
static_assert(VM_RAM_LIMIT==0x20280000, "RAM2 ends at 0x20280000");
// Memory profiles. 0 gives the module all 512 KiB of RAM2. 1 keeps the upper
// 80 KiB of its arena as initialized, non-executable, write-protected constants
// loaded from the image and holds back the reserved top, leaving 416 KiB.
// Profile 2 is reserved (see VM_PROFILE_RESERVED_AUX below) and not loadable.
enum : uint32_t { VM_PROFILE_LEGACY=0, VM_PROFILE_RAM2_RO=1,
                  VM_PROFILE_RESERVED_AUX=2,
                  VM_RAM2_RO_BYTES=80*1024,
                  VM_RAM2_GUEST_BYTES=VM_RAM_BYTES-VM_RAM_RESERVED_BYTES-VM_RAM2_RO_BYTES,
                  VM_RAM2_RO_BASE=VM_RAM_BASE+VM_RAM2_GUEST_BYTES };
static_assert(VM_RAM2_RO_BASE==0x20268000 && VM_RAM2_GUEST_BYTES==416*1024,
              "profile 1 constants are the MPU subregions 2..6 of the 128 KiB window at 0x20260000");
struct VmImageHeader {
    uint32_t magic, abi, header_bytes, code_bytes, data_bytes, bss_bytes;
    uint32_t entry, code_base, ram_base, required_services, payload_crc, header_crc;
    // [0] profile, [1] RAM2 constant payload bytes, [2..3] zero.
    uint32_t reserved[4];
};
static_assert(sizeof(VmImageHeader)==64, "MVM1 image header");
struct VmFileInfo { uint32_t bytes; uint8_t directory; char name[96]; uint8_t attributes; uint16_t date,time; };
struct VmInput { uint8_t buttons, display, overflow, protocol; };
struct VmPacket { uint8_t type, flags, length, reserved; uint8_t payload[228]; };
enum : uint32_t { VM_OPEN_READ=1,VM_OPEN_WRITE=2,VM_OPEN_CREATE=4,VM_OPEN_EXCLUSIVE=8,VM_OPEN_TRUNCATE=16 };
enum class VmFsOp : uint32_t { Flush,Truncate,Timestamp,Close,Mkdir,Rmdir,Remove,Rename,Space };
struct VmFsRequest { VmFsOp operation; uint32_t handle,value,extra; const char *path,*destination; };
// Services the host lends the module. Every pointer below is valid until reset.
// A module must check host->bytes before reading any field, and must not assume
// a callback exists because the corresponding service bit is set on some other
// host -- bits and layout are independent.
struct VmHost {
    uint32_t abi, bytes, services;
    // RAM1 workspace, placed immediately after the module's own data/BSS.
    uint8_t *workspace; uint32_t workspace_bytes;
    // package_root is the module's own /VMS/<id> directory. content_path is the
    // file the user selected, or an empty string when launched by client CRT.
    const char *package_root, *content_path;
    uint32_t (*micros_now)();
    // Handles 1..24; zero is failure. read returns -1 on error.
    uint32_t (*open)(const char *path, VmFileInfo *info);
    int32_t (*read)(uint32_t handle, uint32_t offset, void *data, uint32_t count);
    int32_t (*next)(uint32_t directory, VmFileInfo *info); // 1 entry, 0 EOF, -1 error
    void (*close)(uint32_t handle);
    // RAM2 guest arena, sized by the image profile. Profile 0 runs it to the
    // physical end of RAM2, over the loader's failure record and the core's
    // CrashReport.
    uint8_t *guest_ram; uint32_t guest_ram_bytes;
    uint32_t (*open_flags)(const char *path,uint32_t flags,VmFileInfo *info);
    int32_t (*write)(uint32_t handle,uint32_t offset,const void *data,uint32_t count);
    // 0 success, -1 failure; Space returns total/free sectors in value/extra,
    // and allocation-unit size (sectors per cluster) in handle.
    int32_t (*file_op)(VmFsRequest *request);
    // Cooperative foreground yield for pending input, ACK, retry or time slice.
    bool (*should_yield)();
    void (*fail)(uint8_t code,uint32_t detail);
    // Tail extensions append here. Check host->bytes, never sizeof(VmHost).
};
// The size of the base profile. A module that needs nothing beyond this header
// requires only host->bytes >= VM_HOST_BASE_BYTES, and so runs unchanged on
// both this loader and any host that has appended callbacks after `fail`.
static constexpr uint32_t VM_HOST_BASE_BYTES=sizeof(VmHost);
#if defined(__arm__)
// The on-target layout is the wire format. Freeze it: a module built against
// an older copy of this header must keep working against a newer firmware.
static_assert(sizeof(VmHost)==76, "ABI 2 base host layout is frozen");
#endif
struct VmModule {
    uint32_t abi, bytes;
    // pump is permitted while awaiting ACK; it must not alter frozen output.
    void (*input)(const VmInput *input);
    void (*pump)();
    bool (*packet)(VmPacket *out);
    void (*ack)();
};
#if defined(__arm__)
static_assert(sizeof(VmModule)==24, "ABI 2 module table is frozen");
#endif
using VmEntry = const VmModule *(*)(const VmHost *host);
// Every module defines exactly one entry point with this macro. The linker
// script places .entry first in the module window; the image header records its
// address. Native conformance builds reuse the same source without the section.
#if defined(__ELF__)
#define VM_MODULE_ENTRY extern "C" __attribute__((section(".entry"), used))
#else
#define VM_MODULE_ENTRY extern "C" __attribute__((used))
#endif
enum : uint32_t { VM_SERVICE_FILES=1, VM_SERVICE_CLOCK=2, VM_SERVICE_PACKETS=4,
                  VM_SERVICE_WRITE=8, VM_SERVICE_GUEST_RAM=16,
                  VM_SERVICE_RAM2_RO=128,
                  // The base profile, which every module may assume.
                  VM_SERVICES=31,
                  VM_HOST_SERVICES=VM_SERVICES|VM_SERVICE_RAM2_RO,
                  VM_KNOWN_SERVICES=VM_HOST_SERVICES,
                  // Assigned to out-of-tree extensions; never reused here.
                  // 32 video, 64 indexed video, 256 indexed raster, 512 RAM1 aux.
                  VM_SERVICES_RESERVED=32|64|256|512,
                  VM_IMAGE_MAGIC=0x314d564d };
static_assert((VM_KNOWN_SERVICES&VM_SERVICES_RESERVED)==0, "reserved service bits stay unimplemented");
static inline uint32_t vm_crc32(const void *data, uint32_t size) {
    auto p=static_cast<const uint8_t *>(data); uint32_t c=~0u;
    while(size--) { c^=*p++; for(unsigned b=0;b<8;b++) c=(c>>1)^((0u-(c&1))&0xedb88320u); }
    return ~c;
}
static inline uint32_t vm_image_ro_bytes(const VmImageHeader &h){return h.reserved[1];}
static inline uint32_t vm_image_guest_bytes(const VmImageHeader &h){return h.reserved[0]==VM_PROFILE_RAM2_RO?uint32_t(VM_RAM2_GUEST_BYTES):uint32_t(VM_RAM_BYTES);}
static inline uint32_t vm_image_payload_bytes(const VmImageHeader &h){return h.code_bytes+h.data_bytes+vm_image_ro_bytes(h);}
static inline bool vm_valid_header(const VmImageHeader &h, uint32_t file_bytes) {
    if(h.reserved[2]||h.reserved[3])return false;
    if(h.reserved[0]==VM_PROFILE_LEGACY){
        if(h.reserved[1]||(h.required_services&VM_SERVICE_RAM2_RO))return false;
    }else if(h.reserved[0]==VM_PROFILE_RAM2_RO){
        if(!(h.required_services&VM_SERVICE_RAM2_RO)||!h.reserved[1]||h.reserved[1]>VM_RAM2_RO_BYTES)return false;
    }else return false;
    if(h.magic!=VM_IMAGE_MAGIC || h.abi!=VM_ABI || h.header_bytes!=sizeof h ||
       h.code_base!=VM_CODE_BASE || h.ram_base!=VM_DATA_BASE || !h.code_bytes ||
       h.code_bytes>VM_CODE_LIMIT-VM_CODE_BASE || h.data_bytes>VM_DATA_BYTES ||
       h.bss_bytes>VM_DATA_BYTES-h.data_bytes || (h.required_services&~VM_KNOWN_SERVICES) ||
       file_bytes!=sizeof h+vm_image_payload_bytes(h) || !(h.entry&1) ||
       (h.entry&~1u)<VM_CODE_BASE || (h.entry&~1u)>=VM_CODE_BASE+h.code_bytes) return false;
    VmImageHeader check=h; check.header_crc=0;
    return vm_crc32(&check,sizeof check)==h.header_crc;
}
