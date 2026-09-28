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
    char text[] = "VM1\nEXAMPLE\nhi\nengine.mvm\nclient.crt\nEND\n";
    VmManifest manifest{};
    assert(vm_manifest_parse(text, "/VMS/EXAMPLE", manifest));
    assert(!strcmp(manifest.id, "EXAMPLE") && manifest.crc);

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
    assert(!vm_module_table_valid(reinterpret_cast<const VmModule *>(VM_DATA_BASE), 4));

    printSlotVerdicts();
    puts("PASS: the published host contract compiles and runs with no TeensyROM include path");
    return 0;
}
