// SPDX-License-Identifier: MIT
// Compiled and run by checkPublishedHeadersStandalone() in
// tools/verify-extensions.mjs, from a directory holding nothing but itself,
// VMABI.h and VMHostABI.h, with no include path into this repository.
//
// Naming a constant or a type here proves only that it survived that build.
// The calls are the behavioural checks.
#include "VMHostABI.h"

#include <cassert>
#include <cstdio>
#include <cstring>

static int reads;

struct CountingReader {
    int read(void *, uint32_t bytes) { reads++; return (int)bytes; }
};

// hostSlotValid() in tools/lib/extension.mjs decides the same question at
// package time that this decides on the device, and neither can see the other.
// Printing a verdict per vector lets checkHostSlotPredicate() in
// tools/verify-extensions.mjs hold the two to the same answers.
static const uint32_t slotVectors[][5] = {
    { 0x42464346u, 0x432000d1u, VM_HOST_SLOT_BASE + 0x1001u, VM_HOST_SLOT_BASE, 0x2000u },
    { 0x42464347u, 0x432000d1u, VM_HOST_SLOT_BASE + 0x1001u, VM_HOST_SLOT_BASE, 0x2000u },
    { 0x42464346u, 0x432000d0u, VM_HOST_SLOT_BASE + 0x1001u, VM_HOST_SLOT_BASE, 0x2000u },
    { 0x42464346u, 0x432000d1u, VM_HOST_SLOT_BASE + 0x1000u, VM_HOST_SLOT_BASE, 0x2000u },
    { 0x42464346u, 0x432000d1u, VM_HOST_SLOT_BASE + 0x0fffu, VM_HOST_SLOT_BASE, 0x2000u },
    { 0x42464346u, 0x432000d1u, VM_HOST_SLOT_BASE + 0x2001u, VM_HOST_SLOT_BASE, 0x2000u },
    { 0x42464346u, 0x432000d1u, VM_HOST_SLOT_BASE + 0x3001u, VM_HOST_SLOT_BASE, 0x4000u },
    { 0x42464346u, 0x432000d1u, VM_HOST_SLOT_BASE + 0x3003u, VM_HOST_SLOT_BASE, 0x4000u },
    { 0x42464346u, 0x432000d1u, VM_HOST_SLOT_BASE + 0x1001u, 0u, 0x2000u },
    { 0x42464346u, 0x432000d1u, VM_HOST_SLOT_BASE + 0x1001u, VM_HOST_SLOT_BASE, 0x1000u },
    { 0x42464346u, 0x432000d1u, VM_HOST_SLOT_BASE + 0x1001u, VM_HOST_SLOT_BASE, 0x1001u },
    { 0x42464346u, 0x432000d1u, VM_HOST_SLOT_BASE + 0x1001u, VM_HOST_SLOT_BASE, VM_HOST_SLOT_BYTES },
    { 0x42464346u, 0x432000d1u, VM_HOST_SLOT_BASE + 0x1001u, VM_HOST_SLOT_BASE, VM_HOST_SLOT_BYTES + 1u },
};

static void printSlotVerdicts() {
    for (const auto &v : slotVectors) {
        printf("SLOTVALID %08x %08x %08x %08x %08x %d\n", v[0], v[1], v[2], v[3], v[4],
               vm_host_slot_valid(v[0], v[1], v[2], v[3], v[4]) ? 1 : 0);
    }
}

int main() {
    // The slot, and the descriptor the main image reads out of it.
    assert(VM_HOST_SLOT_BYTES == VM_HOST_SLOT_LIMIT - VM_HOST_SLOT_BASE);
    assert(vm_host_slot_valid(0x42464346u, 0x432000d1u, VM_HOST_SLOT_BASE + 0x1001u,
                              VM_HOST_SLOT_BASE, 0x2000u));
    assert(!vm_host_slot_valid(0u, 0x432000d1u, VM_HOST_SLOT_BASE + 0x1001u,
                               VM_HOST_SLOT_BASE, 0x2000u));
    const VmHostId id = { VM_HOSTID_MAGIC, VM_ABI, VM_HOST_SERVICES, VM_HOST_BASE_BYTES, "Example", 0 };
    assert(id.magic == VM_HOSTID_MAGIC && sizeof id == 32);

    // Entry and exit.
    assert(!strcmp(VM_HOST_MARKER, "@VM1"));
    assert(VM_EEP_MAGIC == 0xfeed6415u && VM_EEP_MAGIC_ADDR == 0);
    assert(VM_EEP_BOOTNAME_ADDR == 1919 && VM_EEP_BOOTIND_ADDR == 2175);
    assert(VM_BOOT_SKIP_MIN != VM_BOOT_FROM_MIN && VM_BOOT_EXECUTE_MIN != VM_BOOT_FROM_MIN);

    // The launch record, round-tripped through its own validator.
    VmLaunchRecord launch{};
    launch.magic = VM_LAUNCH_MAGIC;
    strcpy(launch.root, "/VMS/EXAMPLE");
    launch.crc = vm_crc32(&launch, offsetof(VmLaunchRecord, crc));
    assert(vm_launch_valid(launch));
    launch.root[0] = 'x';
    assert(!vm_launch_valid(launch));

    // The manifest, and the path rules it is built on.
    assert(vm_path_absolute("/VMS/EXAMPLE", 80) && !vm_path_absolute("/VMS/../etc", 80));
    assert(vm_path_component("EXAMPLE") && !vm_path_component("a/b"));
    assert(vm_manifest_extensions("hi") && !vm_manifest_extensions("prg"));
    // The list may run to the field less its NUL; each extension stays under 8.
    assert(vm_manifest_extensions("a26,a52,a78,nes,gb,gbc,gg,sms,x"));
    assert(!vm_manifest_extensions("a26,a52,a78,nes,gb,gbc,gg,sms,xy"));
    assert(!vm_manifest_extensions("abcdefgh"));
    char text[] = "VM1\nEXAMPLE\nhi\nengine.mvm\nclient.crt\nEND\n";
    // The CRC is the file's bytes as read, taken before the parse writes NULs into them.
    const uint32_t fileCrc = vm_crc32(text, strlen(text));
    VmManifest manifest{};
    assert(vm_manifest_parse(text, "/VMS/EXAMPLE", manifest));
    assert(!strcmp(manifest.id, "EXAMPLE") && manifest.crc == fileCrc);

    // The failure record.
    VmFailRecord fail{};
    vm_fail_fill(fail, VM_FAIL_VENDOR_BASE, 7);
    assert(vm_fail_intact(&fail) && fail.magic == VM_FAIL_MAGIC);
    fail.detail ^= 1u;
    assert(!vm_fail_intact(&fail));
    assert(VM_FAIL_BASE % 32 == 0);

    // What a host will serve. The refusal a module sees when a host lacks one
    // of its services is this predicate, not the image validator.
    VmImageHeader wants{};
    wants.required_services = VM_SERVICES | VM_SERVICE_RAM2_RO;
    assert(vm_host_serves(wants, VM_HOST_SERVICES));
    assert(!vm_host_serves(wants, VM_SERVICES));
    wants.required_services = VM_SERVICES | 0x10000u;
    assert(!vm_host_serves(wants, VM_HOST_SERVICES));
    assert(vm_host_serves(wants, VM_HOST_SERVICES | 0x10000u));

    // Loading a module. vm_module_table_valid gets no accept case: the code
    // window is a fixed ITCM address no native allocation can land on.
    VmImageHeader image{};
    image.code_bytes = 4;
    uint8_t code[4] = {}, data[4] = {};
    uint8_t failure = 0;
    CountingReader reader;
    assert(!vm_load_payload(image, reader, code, data, nullptr, failure));
    assert(failure == 0x13 && reads == 1);
    assert(!vm_module_table_valid(reinterpret_cast<const VmModule *>(VM_DATA_BASE), VM_CODE_BASE, 4));

    // The code floor. Zero is a host that never stated one, and reads as the
    // base every host accepts rather than as "anything goes".
    VmImageHeader narrow{}, wide{};
    narrow.code_base = VM_CODE_BASE;
    wide.code_base = VM_CODE_BASE_128K;
    assert(vm_host_code_floor(0) == VM_CODE_BASE);
    assert(vm_host_code_floor(0xdd1cu) == 0xdd1cu);
    assert(vm_host_takes_code(narrow, 0) && !vm_host_takes_code(wide, 0));
    assert(vm_host_takes_code(narrow, VM_CODE_BASE) && !vm_host_takes_code(wide, VM_CODE_BASE));
    // A real 64 KiB link ends short of the wide base, so it takes both.
    assert(vm_host_takes_code(narrow, 0xdd1cu) && vm_host_takes_code(wide, 0xdd1cu));
    assert(vm_host_takes_code(narrow, VM_CODE_BASE_128K) && vm_host_takes_code(wide, VM_CODE_BASE_128K));
    // A host whose code runs past the narrow base can take neither. The link
    // ASSERT stops that being built; the predicate still has to answer safely.
    assert(!vm_host_takes_code(narrow, VM_CODE_BASE + 1) && !vm_host_takes_code(wide, VM_CODE_BASE + 1));

    // Both windows split into MPU-legal regions: each a power of two, each
    // based on a multiple of its own size, together covering the window with
    // no gap. A base the validator does not allow would break this, which is
    // why it allows exactly two.
    const uint32_t bases[2] = { VM_CODE_BASE, VM_CODE_BASE_128K };
    for (uint32_t base : bases) {
        uint32_t next = base;
        for (unsigned i = 0; i < 2; i++) {
            const VmCodeRegion r = vm_code_window_region(base, i);
            assert(r.base == next && r.bytes >= 32);
            assert((r.bytes & (r.bytes - 1)) == 0);
            assert(r.base % r.bytes == 0);
            assert((2u << vm_mpu_size_field(r.bytes)) == r.bytes);
            next = r.base + r.bytes;
        }
        assert(next == VM_CODE_LIMIT);
    }
    assert(vm_mpu_size_field(32) == 4 && vm_mpu_size_field(0x8000) == 14 &&
           vm_mpu_size_field(0x10000) == 15);

    printSlotVerdicts();
    puts("PASS: the published host contract compiles and runs with no TeensyROM include path");
    return 0;
}
