// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>
#include "VMABI.h"

// TeensyROM extension HOST contract, ABI 2.
//
// VMABI.h is what a module compiles against. This is what a *host* compiles
// against: the image that occupies the extension flash slot, is entered by the
// minimal boot image, loads a module from the SD card and runs it. Copy the
// two headers next to your sources: checkPublishedHeadersStandalone() in
// tools/verify-extensions.mjs builds them that way, from a directory holding
// nothing else, so that they stay sufficient on their own.
//
// What this header does NOT cover is TeensyROM's own implementation: the
// EasyFlash client cartridge and its VMH1 descriptor, the IO2 register link,
// the packet wire format, and the scheduler. A host may provide all of that,
// some of it, or none; the base profile's VM_SERVICE_PACKETS bit is how it
// says which. See vm/abi/README.md.
//
// Core dependencies. vm_host_code_window() below is #if defined(__arm__) and
// uses the Teensy core's SCB_MPU_* and __disable_irq(); VM_HOST_TEXT picks up
// the core's FLASHMEM when it is defined. The rest is the four standard
// headers above; strings.h is there for strcasecmp, which is POSIX rather
// than C, so a toolchain without it needs its own shim.

// Placement. The extension image has only 96 KiB of ITCM, so TeensyROM keeps
// the larger pure helpers in flash; FLASHMEM is the Teensy core's attribute
// for that, and it must already be defined when this header is included to be
// picked up. Define VM_HOST_TEXT yourself to choose otherwise.
#ifndef VM_HOST_TEXT
#  ifdef FLASHMEM
#    define VM_HOST_TEXT FLASHMEM
#  else
#    define VM_HOST_TEXT
#  endif
#endif

// ---------------------------------------------------------------- the slot

// Must match VM_BASE/VM_LIMIT in tools/lib/hex.mjs, which partitions the
// combined hex; checkBootSlot() in tools/verify-extensions.mjs compares them.
enum : uint32_t { VM_HOST_SLOT_BASE = 0x60280000u,
                  VM_HOST_SLOT_LIMIT = 0x602e0000u,
                  VM_HOST_SLOT_BYTES = VM_HOST_SLOT_LIMIT - VM_HOST_SLOT_BASE,
                  // In the 0xFF fill between the FlexSPI configuration block
                  // and the image vector table, so the descriptor costs no
                  // image space. extensionLinkerScript() places it here.
                  VM_HOST_ID_OFFSET = 0x800u };

// The five words the minimal image reads out of the slot before it will jump.
// A host image that fails any of these is never entered, and the menu reports
// it as no host installed -- so an installer should apply this to a candidate
// before it erases anything.
static inline bool vm_host_slot_valid(uint32_t flashMagic, uint32_t vectorMagic,
                                      uint32_t entry, uint32_t bootBase, uint32_t imageBytes) {
    const uint32_t address = entry & ~1u;
    return flashMagic == 0x42464346u && vectorMagic == 0x432000d1u &&
           (entry & 1u) && address >= VM_HOST_SLOT_BASE + 0x1000u &&
           address <= VM_HOST_SLOT_BASE + 0x3000u &&
           bootBase == VM_HOST_SLOT_BASE && imageBytes > 0x1000u &&
           imageBytes <= VM_HOST_SLOT_BYTES;
}

// Stamped into the image at VM_HOST_ID_OFFSET so the main image can read what
// the installed host provides without booting it. services is the host's
// provided set; name is for the refusal message on the C64.
struct VmHostId {
    uint32_t magic, abi, services, host_bytes;
    char name[12];
    uint32_t reserved;
};
static_assert(sizeof(VmHostId)==32, "MVH2 host descriptor");
enum : uint32_t { VM_HOSTID_MAGIC = 0x3248564du };  // 'MVH2'

// ------------------------------------------------------------ entry and exit

// Written to eepAdCrtBootName in place of a file path when the selection was
// an extension. It is what authorizes a host to run, and it is never consumed:
// only the boot indicator makes the minimal image act on it.
#define VM_HOST_MARKER "@VM1"

// Duplicated from Common_Defs.h, which is firmware-wide and must not be
// included by a host. checkEepromProtocol() in verify-extensions.mjs compares
// them.
enum : uint32_t { VM_EEP_MAGIC = 0xfeed6415u };
enum : int { VM_EEP_MAGIC_ADDR = 0, VM_EEP_BOOTNAME_ADDR = 1919, VM_EEP_BOOTIND_ADDR = 2175 };

// The boot indicator, which the main image reads on the way back. A host must
// leave VM_BOOT_FROM_MIN behind when it resets to the menu; VM_BOOT_SKIP_MIN
// reads there as a cold power up and re-runs the user's autolaunch file.
// VM_BOOT_EXECUTE_MIN is the minimal image's to consume and has already been
// replaced by VM_BOOT_FROM_MIN by the time a host runs. Common_Defs.h defines
// a fourth value, MinBootInd_LaunchFull, outside this contract.
enum : uint8_t { VM_BOOT_SKIP_MIN = 0, VM_BOOT_EXECUTE_MIN = 1, VM_BOOT_FROM_MIN = 2 };

// ------------------------------------------------------------ launch record

// /VMS/launch.vml, written by the main image before it reboots and read by the
// host afterwards. Fixed size: a file of any other length is refused.
struct VmLaunchRecord {
    uint32_t magic, manifest_crc;
    char root[80], content[256];
    uint32_t crc;
};
static_assert(sizeof(VmLaunchRecord)==348, "VML1 launch record");
enum : uint32_t { VM_LAUNCH_MAGIC = 0x314c4d56u };  // 'VML1'

// Every path the module side may name, and the sandbox boundary for its file
// services. Absolute, NUL-terminated within cap, no traversal, no backslash.
VM_HOST_TEXT static bool vm_path_absolute(const char *s, size_t cap) {
    if(!s||s[0]!='/'||!memchr(s,0,cap)||strstr(s,"..")||strchr(s,'\\'))return false;
    return true;
}

// One path component: a directory or file name, no separators.
VM_HOST_TEXT static bool vm_path_component(const char *s) {
    if(!*s || strstr(s,".."))return false;
    for(;*s;s++)if(!((*s>='A'&&*s<='Z')||(*s>='a'&&*s<='z')||(*s>='0'&&*s<='9')||*s=='_'||*s=='-'||*s=='.'))return false;
    return true;
}

static inline bool vm_launch_valid(const VmLaunchRecord &l) {
    return l.magic==VM_LAUNCH_MAGIC && l.crc==vm_crc32(&l,offsetof(VmLaunchRecord,crc)) &&
           vm_path_absolute(l.root,sizeof l.root) &&
           (!l.content[0]||vm_path_absolute(l.content,sizeof l.content));
}

// ---------------------------------------------------------------- manifest

// <root>/manifest.vmi, parsed into this and CRC'd. The CRC is the only value
// that has to agree across the reboot: the main image computes it before
// launching and the host recomputes it, so a package edited in between is
// caught. It runs over the struct including its padding.
struct VmManifest { char id[24],extension[8],module[32],client[32];uint32_t crc; };

// Extensions the stock menu owns, which no package may claim. A host does not
// enforce this -- discovery happens in the main image -- but the parser must
// agree byte for byte or the CRC will not match.
VM_HOST_TEXT static bool vm_manifest_extensions(const char *list) {
    if(!*list||strlen(list)>7)return false;
    static const char protectedExtensions[][4]={"prg","crt","hex","p00","sid","kla","koa","ocp","pic","art","aas","hpi","txt","nfo","md","seq","d64","d71","d81","reu","trh"};
    for(const char *p=list;*p;){char ext[8]{};const char *end=strchr(p,',');size_t n=end?size_t(end-p):strlen(p);
        if(!n||n>=sizeof ext)return false;memcpy(ext,p,n);if(!vm_path_component(ext)||strchr(ext,'.'))return false;
        for(const auto &protectedExt:protectedExtensions)if(!strcasecmp(ext,protectedExt))return false;
        if(!end)return true;p=end+1;if(!*p)return false;}return false;
}

// Six strict lines: VM1, id, extensions, module, client, END. `text` is the
// whole file, NUL-terminated and writable; `root` is /VMS/<id>, whose last
// component must equal the id. Returns false without touching `out` on any
// malformed input.
VM_HOST_TEXT static inline bool vm_manifest_parse(char *text, const char *root, VmManifest &out) {
    if(!vm_path_absolute(root,80))return false;
    char *line[6],*p=text;unsigned count=0;
    while(*p&&count<6){line[count++]=p;while(*p&&*p!='\n'&&*p!='\r')p++;if(*p){*p++=0;while(*p=='\n'||*p=='\r')p++;}}
    if(count!=6||*p||strcmp(line[0],"VM1")||strcmp(line[5],"END"))return false;
    if(!vm_path_component(line[1])||!vm_manifest_extensions(line[2])||!vm_path_component(line[3])||!vm_path_component(line[4])||
       strlen(line[1])>=sizeof out.id||strlen(line[2])>=sizeof out.extension||strlen(line[3])>=sizeof out.module||strlen(line[4])>=sizeof out.client)return false;
    const char *id=strrchr(root,'/');if(!id||strcmp(id+1,line[1]))return false;
    memset(&out,0,sizeof out);strcpy(out.id,line[1]);strcpy(out.extension,line[2]);strcpy(out.module,line[3]);strcpy(out.client,line[4]);
    out.crc=vm_crc32(&out,offsetof(VmManifest,crc));return true;
}

// ------------------------------------------------------------ failure record

// The host image is built USB_DISABLED and its only exit is a reset, so a
// failure inside it is otherwise silent. It leaves one record at a fixed
// address in the cache line directly below the core's own CrashReport, which
// survives the soft reset; the main image collects it and shows it on the
// menu. Padded to a cache line, crc last so it covers every other word
// including padding.
struct VmFailRecord { uint32_t magic, code, detail; uint32_t reserved[4]; uint32_t crc; };
static_assert(sizeof(VmFailRecord)==32, "one cache line");
enum : uint32_t { VM_FAIL_MAGIC = 0x3146564du };  // 'MVF1'
// Fixed, not derived from the arena: profile 0 lends the guest all of RAM2, so
// the record has to sit at one address whatever the profile. A guest that
// overwrites it fails the gate below and reads as no record at all.
enum : uint32_t { VM_FAIL_BASE = 0x2027FF60u };
static_assert(VM_FAIL_BASE % 32 == 0, "a cache line of its own");
static_assert(VM_FAIL_BASE + sizeof(VmFailRecord) == 0x2027FF80u, "directly below the core CrashReport");

// 0x00..0x2f are TeensyROM's; 0x30..0x7f are reserved for it. A host defines
// its own from VM_FAIL_VENDOR_BASE up, and the menu reports those generically
// with the detail word in hex.
enum : uint8_t { VM_FAIL_VENDOR_BASE = 0x80 };

static inline void vm_fail_fill(VmFailRecord &r, uint8_t code, uint32_t detail) {
    r = VmFailRecord{ VM_FAIL_MAGIC, code, detail, {}, 0 };
    r.crc = vm_crc32(&r, offsetof(VmFailRecord, crc));
}

static inline bool vm_fail_intact(const VmFailRecord *r) {
    return r->magic == VM_FAIL_MAGIC && r->crc == vm_crc32(r, offsetof(VmFailRecord, crc));
}

// ------------------------------------------------------------ loading a module

// A host publishes the services it provides; a module names the set it cannot
// run without. An image requiring anything outside that set is refused whole,
// never loaded with the service missing. The main image asks the same question
// by name before it reboots, but it can only ask a host whose descriptor says
// what it provides -- so the host owes this check whatever happened earlier.
static inline bool vm_host_serves(const VmImageHeader &h, uint32_t provided) {
    return (h.required_services & ~provided) == 0;
}

// The module's ITCM window is read-only at entry, because the core's MPU
// region 1 covers all of ITCM, so the payload copy faults without this. Call
// with true before copying code and false after, which also restores execute
// permission.
#if defined(__arm__)
static inline void vm_host_code_window(bool writable) {
    uint32_t mask; __asm__ volatile("mrs %0, primask":"=r"(mask)); __disable_irq();
    __asm__ volatile("dsb":::"memory"); SCB_MPU_CTRL = 0;
    // 96 KiB window: 32 KiB at 0x18000, then 64 KiB at 0x20000.
    for (unsigned i = 0; i < 2; i++) {
        SCB_MPU_RBAR = (i ? 0x20000u : 0x18000u) | SCB_MPU_RBAR_VALID | (11 + i);
        SCB_MPU_RASR = SCB_MPU_RASR_TEX(1) | SCB_MPU_RASR_AP(writable ? 3 : 7) |
            (writable ? SCB_MPU_RASR_XN : 0) | SCB_MPU_RASR_SIZE(i ? 15 : 14) | SCB_MPU_RASR_ENABLE;
    }
    SCB_MPU_CTRL = SCB_MPU_CTRL_ENABLE; __asm__ volatile("dsb\nisb":::"memory");
    if (!mask) __enable_irq();
}
#endif

// Streams .text, .data and the profile-1 constants from an open image, CRCs
// the three together against the header, and zeroes .bss. `h` must have passed
// vm_valid_header (VMABI.h) already: nothing here bounds code_bytes, data_bytes
// or bss_bytes, and the payload CRC is only checked once the copies are done. Reader supplies
// int read(void *, uint32_t). Failure codes match the module-load detail the
// menu reports: 0x12 short read, 0x13 payload CRC.
template<class Reader> static bool vm_load_payload(const VmImageHeader &h,Reader &reader,
        uint8_t *code,uint8_t *data,uint8_t *ro,uint8_t &failure){
    uint32_t crc=~0u;
    for(unsigned part=0;part<3;part++){
        auto p=part==0?code:part==1?data:ro;
        uint32_t n=part==0?h.code_bytes:part==1?h.data_bytes:vm_image_ro_bytes(h);
        if(n&&(!p||reader.read(p,n)!=(int)n)){failure=0x12;return false;}
        while(n--){crc^=*p++;for(unsigned b=0;b<8;b++)crc=(crc>>1)^((0u-(crc&1))&0xedb88320u);}
    }
    if(~crc!=h.payload_crc){failure=0x13;return false;}
    memset(data+h.data_bytes,0,h.bss_bytes);return true;
}

// The table vm_entry returned. Every pointer in it must lie inside the code
// actually loaded.
static inline bool vm_module_table_valid(const VmModule *module, uint32_t code_bytes) {
    const uintptr_t end = VM_CODE_BASE + code_bytes;
    auto codePointer = [end](uintptr_t p) { return (p & 1) && (p & ~1u) >= VM_CODE_BASE && (p & ~1u) < end; };
    const uintptr_t p = (uintptr_t)module;
    return p >= VM_CODE_BASE && p <= VM_CODE_LIMIT - sizeof(VmModule) && module->abi == VM_ABI &&
           module->bytes == sizeof(VmModule) && codePointer((uintptr_t)module->input) &&
           codePointer((uintptr_t)module->pump) && codePointer((uintptr_t)module->packet) &&
           codePointer((uintptr_t)module->ack);
}
