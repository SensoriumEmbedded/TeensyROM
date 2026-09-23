// SPDX-License-Identifier: MIT
//
// Writing an extension host into the slot at runtime, from a .TRH package on
// the SD card. tools/lib/extension.mjs writes what this reads.

#ifndef VMHOSTINSTALL_H
#define VMHOSTINSTALL_H

#include <stdint.h>
#include <string.h>

#include "VMHostABI.h"

enum : uint32_t { VM_TRH_MAGIC = 0x31485254u,   // 'TRH1'
                  VM_TRH_FORMAT = 1u,
                  VM_TRH_HEADER_BYTES = 64u,
                  VM_HOST_SECTOR_BYTES = 0x1000u,
                  VM_HOST_SECTORS = VM_HOST_SLOT_BYTES / VM_HOST_SECTOR_BYTES,
                  VM_HOST_PAGE_BYTES = 256u,
                  // vm_host_scan reads the boot words out of sector 1 of a
                  // staging buffer it fills a sector at a time, so a payload
                  // stopping inside that sector would be judged on what sector
                  // 0 left behind. tools/lib/extension.mjs writes to the same
                  // bound.
                  VM_HOST_MIN_PAYLOAD_BYTES = 2u * VM_HOST_SECTOR_BYTES,
                  VM_FLASH_TAG = 0x42464346u };

enum class VmInstallStatus : uint8_t {
    Ok, ShortFile, BadMagic, BadFormat, BadHeader, BadHeaderCrc, WrongSlot,
    BadLength, ReadError, BadPayloadCrc, MirrorMismatch, WrongAbi,
    NotBootable, EraseFailed, ProgramFailed, VerifyFailed,
};

struct VmInstallResult {
    VmInstallStatus status;
    uint32_t detail;
    operator bool() const { return status == VmInstallStatus::Ok; }
};

struct VmTrhHeader {
    uint32_t magic, format, headerBytes, targetBase, targetBytes, payloadBytes,
             imageBytes, entry, abi, services, payloadCrc, headerCrc, reserved[4];
};
static_assert(sizeof(VmTrhHeader) == VM_TRH_HEADER_BYTES, "TRH1 container header");

// CRC-32, the same polynomial and seed tools/lib/extension.mjs uses.
static inline uint32_t vm_crc32(uint32_t c, const uint8_t *p, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int b = 0; b < 8; b++) c = (c >> 1) ^ (0xedb88320u & (uint32_t)(-(int32_t)(c & 1)));
    }
    return c;
}
static inline uint32_t vm_crc32_begin() { return 0xffffffffu; }
static inline uint32_t vm_crc32_end(uint32_t c) { return c ^ 0xffffffffu; }

// Validates the TRH1 container header alone, before anything past it is read.
static inline VmInstallResult vm_trh_valid(const VmTrhHeader &h, uint32_t fileBytes) {
    if (fileBytes < VM_TRH_HEADER_BYTES) return { VmInstallStatus::ShortFile, fileBytes };
    if (h.magic != VM_TRH_MAGIC) return { VmInstallStatus::BadMagic, h.magic };
    if (h.format != VM_TRH_FORMAT) return { VmInstallStatus::BadFormat, h.format };
    if (h.headerBytes != VM_TRH_HEADER_BYTES) return { VmInstallStatus::BadHeader, h.headerBytes };
    if (h.targetBase != VM_HOST_SLOT_BASE) return { VmInstallStatus::WrongSlot, h.targetBase };
    if (h.targetBytes != VM_HOST_SLOT_BYTES) return { VmInstallStatus::WrongSlot, h.targetBytes };
    if (h.reserved[0] || h.reserved[1] || h.reserved[2] || h.reserved[3]) {
        return { VmInstallStatus::BadHeader, h.reserved[0] };
    }
    if (h.payloadBytes < VM_HOST_MIN_PAYLOAD_BYTES || h.payloadBytes > VM_HOST_SLOT_BYTES) {
        return { VmInstallStatus::BadLength, h.payloadBytes };
    }
    if (fileBytes != VM_TRH_HEADER_BYTES + h.payloadBytes) return { VmInstallStatus::BadLength, fileBytes };

    VmTrhHeader zeroed = h;
    zeroed.headerCrc = 0;
    const uint32_t crc = vm_crc32_end(vm_crc32(vm_crc32_begin(), (const uint8_t *)&zeroed, sizeof zeroed));
    if (crc != h.headerCrc) return { VmInstallStatus::BadHeaderCrc, crc };
    return { VmInstallStatus::Ok, 0 };
}

struct VmHostCandidate {
    uint32_t payloadCrc, bodyCrc;      // whole payload; and 0x1000..end, which vm_host_install re-reads
    uint32_t bootWords[5];             // flashMagic, vectorMagic, entry, bootBase, imageBytes
    VmHostId id;
};

// Reader must provide: bool read(void *dst, uint32_t n).
// Staging is the caller's 4 KiB buffer -- one sector, an exact multiple of the
// 256-byte page, and 8 SD blocks.
template<class Reader>
static VmInstallResult vm_host_scan(Reader &reader, const VmTrhHeader &h,
                                    uint8_t *staging, VmHostCandidate &out) {
    if (h.payloadBytes < VM_HOST_MIN_PAYLOAD_BYTES || h.payloadBytes > VM_HOST_SLOT_BYTES) {
        return { VmInstallStatus::BadLength, h.payloadBytes };
    }

    uint32_t payload = vm_crc32_begin(), body = vm_crc32_begin();
    memset(&out, 0, sizeof out);

    for (uint32_t off = 0; off < h.payloadBytes; off += VM_HOST_SECTOR_BYTES) {
        const uint32_t n = h.payloadBytes - off < VM_HOST_SECTOR_BYTES ? h.payloadBytes - off : VM_HOST_SECTOR_BYTES;
        if (!reader.read(staging, n)) return { VmInstallStatus::ReadError, off };
        payload = vm_crc32(payload, staging, n);
        if (off >= 0x1000u) body = vm_crc32(body, staging, n);

        if (off == 0) {
            memcpy(&out.bootWords[0], staging + 0x0, 4);
            memcpy(&out.id, staging + VM_HOST_ID_OFFSET, sizeof out.id);
        } else if (off == 0x1000u) {
            memcpy(&out.bootWords[1], staging + 0x0, 4);
            memcpy(&out.bootWords[2], staging + 0x4, 4);
            memcpy(&out.bootWords[3], staging + 0x20, 4);
            memcpy(&out.bootWords[4], staging + 0x24, 4);
        }
    }
    out.payloadCrc = vm_crc32_end(payload);
    out.bodyCrc = vm_crc32_end(body);

    if (out.payloadCrc != h.payloadCrc) return { VmInstallStatus::BadPayloadCrc, out.payloadCrc };
    if (out.bootWords[4] != h.imageBytes) return { VmInstallStatus::MirrorMismatch, out.bootWords[4] };
    if (out.bootWords[2] != h.entry) return { VmInstallStatus::MirrorMismatch, out.bootWords[2] };
    if (out.id.magic != VM_HOSTID_MAGIC) return { VmInstallStatus::MirrorMismatch, out.id.magic };
    if (out.id.abi != h.abi) return { VmInstallStatus::MirrorMismatch, out.id.abi };
    if (out.id.services != h.services) return { VmInstallStatus::MirrorMismatch, out.id.services };
    if (out.id.abi != VM_ABI) return { VmInstallStatus::WrongAbi, out.id.abi };
    if (h.payloadBytes != out.bootWords[4]) return { VmInstallStatus::BadLength, h.payloadBytes };
    if (!vm_host_slot_valid(out.bootWords[0], out.bootWords[1], out.bootWords[2],
                            out.bootWords[3], out.bootWords[4])) {
        return { VmInstallStatus::NotBootable, out.bootWords[1] };
    }
    return { VmInstallStatus::Ok, 0 };
}

// Flash must provide:
//   bool erase(uint32_t sector)                          -- 4 KiB, sector 0 is the slot base
//   bool program(uint32_t offset, const uint8_t *, uint32_t n)
//   const uint8_t *map(uint32_t offset)                  -- reads back what is programmed
// program() is given page-aligned runs no longer than a page; splitting is
// done here because the page-program command wraps within its page and the
// primitive does not check.
template<class Flash>
static VmInstallResult vm_host_program(Flash &flash, uint32_t offset, const uint8_t *data, uint32_t n) {
    while (n) {
        const uint32_t room = VM_HOST_PAGE_BYTES - (offset % VM_HOST_PAGE_BYTES);
        const uint32_t take = n < room ? n : room;
        bool blank = true;
        for (uint32_t i = 0; i < take; i++) if (data[i] != 0xff) { blank = false; break; }
        if (!blank && !flash.program(offset, data, take)) return { VmInstallStatus::ProgramFailed, offset };
        offset += take; data += take; n -= take;
    }
    return { VmInstallStatus::Ok, 0 };
}

// Stops the slot reading as a host, before the sector holding the tag is
// erased. A sector erase gives no order in which its cells reach the erased
// state, so an interrupted one can leave the tag standing over a wiped
// descriptor; a program can only clear bits, so a torn one cannot.
template<class Flash>
static VmInstallResult vm_host_invalidate(Flash &flash) {
    static const uint8_t cleared[4] = { 0, 0, 0, 0 };
    if (!flash.program(0, cleared, sizeof cleared)) return { VmInstallStatus::ProgramFailed, 0 };
    if (!flash.erase(0)) return { VmInstallStatus::EraseFailed, 0 };
    return { VmInstallStatus::Ok, 0 };
}

// Erases the slot and writes the package into it. The reader must already be
// positioned at the payload, and is rewound to it again for the second pass.
//
// Reader must also provide: bool seek(uint32_t absoluteOffset).
template<class Flash, class Reader>
static VmInstallResult vm_host_install(Flash &flash, Reader &reader, const VmTrhHeader &h,
                                       const VmHostCandidate &candidate, uint8_t *staging) {
    const VmInstallResult cleared = vm_host_invalidate(flash);
    if (!cleared) return cleared;
    for (uint32_t s = 1; s < VM_HOST_SECTORS; s++) {
        if (!flash.erase(s)) return { VmInstallStatus::EraseFailed, s };
    }

    if (!reader.seek(VM_TRH_HEADER_BYTES)) return { VmInstallStatus::ReadError, 0 };
    if (!reader.read(staging, VM_HOST_SECTOR_BYTES)) return { VmInstallStatus::ReadError, 0 };
    uint8_t sector0[VM_HOST_SECTOR_BYTES];
    memcpy(sector0, staging, VM_HOST_SECTOR_BYTES);

    for (uint32_t off = VM_HOST_SECTOR_BYTES; off < h.payloadBytes; off += VM_HOST_SECTOR_BYTES) {
        const uint32_t n = h.payloadBytes - off < VM_HOST_SECTOR_BYTES ? h.payloadBytes - off : VM_HOST_SECTOR_BYTES;
        if (!reader.read(staging, n)) return { VmInstallStatus::ReadError, off };
        const VmInstallResult r = vm_host_program(flash, off, staging, n);
        if (!r) return r;
    }

    uint32_t body = vm_crc32_begin();
    for (uint32_t off = VM_HOST_SECTOR_BYTES; off < h.payloadBytes; off += VM_HOST_SECTOR_BYTES) {
        const uint32_t n = h.payloadBytes - off < VM_HOST_SECTOR_BYTES ? h.payloadBytes - off : VM_HOST_SECTOR_BYTES;
        body = vm_crc32(body, flash.map(off), n);
    }
    if (vm_crc32_end(body) != candidate.bodyCrc) return { VmInstallStatus::VerifyFailed, vm_crc32_end(body) };

    // Everything but the tag: the tag at offset 0 is what vm_host_slot_valid
    // gates on, so it is programmed last, after the descriptor beside it.
    const VmInstallResult r = vm_host_program(flash, 4, sector0 + 4, VM_HOST_SECTOR_BYTES - 4);
    if (!r) return r;

    uint32_t tag;
    memcpy(&tag, sector0, 4);
    if (!flash.program(0, (const uint8_t *)&tag, 4)) return { VmInstallStatus::ProgramFailed, 0 };

    uint32_t whole = vm_crc32_begin();
    for (uint32_t off = 0; off < h.payloadBytes; off += VM_HOST_SECTOR_BYTES) {
        const uint32_t n = h.payloadBytes - off < VM_HOST_SECTOR_BYTES ? h.payloadBytes - off : VM_HOST_SECTOR_BYTES;
        whole = vm_crc32(whole, flash.map(off), n);
    }
    if (vm_crc32_end(whole) != h.payloadCrc) {
        vm_host_invalidate(flash);   // so a failed install reads as no host rather than a bad one
        return { VmInstallStatus::VerifyFailed, vm_crc32_end(whole) };
    }
    return { VmInstallStatus::Ok, h.payloadBytes };
}

// True when the slot holds something the minimal image would enter.
template<class Flash>
static bool vm_host_installed(Flash &flash) {
    uint32_t w[5];
    memcpy(&w[0], flash.map(0x0), 4);
    memcpy(&w[1], flash.map(0x1000), 4);
    memcpy(&w[2], flash.map(0x1004), 4);
    memcpy(&w[3], flash.map(0x1020), 4);
    memcpy(&w[4], flash.map(0x1024), 4);
    return vm_host_slot_valid(w[0], w[1], w[2], w[3], w[4]);
}

#endif
