// SPDX-License-Identifier: MIT
// Runs the firmware's installer against a real .TRH package built by
// tools/lib/extension.mjs, so the JavaScript writer and the C++ reader are
// checked against each other rather than each against itself.
//
// The power-loss sweep at the end cuts every operation an install performs in
// turn, and requires the slot to read as absent, as the old host, or as the
// new one.
#include <cassert>
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdio>
#include "fake_flash.h"

static VmTrhHeader header_of(const std::vector<uint8_t> &pkg) {
    VmTrhHeader h; memcpy(&h, pkg.data(), sizeof h); return h;
}

// A slot already holding a different, valid host, so the sweep can tell "the
// old one survived" from "the new one landed" from "neither".
static std::vector<uint8_t> incumbent(uint32_t bytes) {
    std::vector<uint8_t> image(bytes, 0x5a);
    auto put = [&](uint32_t off, uint32_t v) { memcpy(&image[off], &v, 4); };
    put(0x0, VM_FLASH_TAG); put(0x1000, 0x432000d1u); put(0x1004, VM_HOST_SLOT_BASE + 0x1801u);
    put(0x1020, VM_HOST_SLOT_BASE); put(0x1024, bytes);
    put(VM_HOST_ID_OFFSET, VM_HOSTID_MAGIC); put(VM_HOST_ID_OFFSET + 4, VM_ABI);
    return image;
}

int main(int argc, char **argv) {
    assert(argc == 2);
    std::ifstream f(argv[1], std::ios::binary);
    std::vector<uint8_t> pkg{std::istreambuf_iterator<char>(f), {}};
    assert(pkg.size() > VM_TRH_HEADER_BYTES);

    const VmTrhHeader good = header_of(pkg);
    assert(vm_trh_valid(good, pkg.size()));

    // Every single-bit flip of the header must be refused, the CRC included.
    for (unsigned i = 0; i < VM_TRH_HEADER_BYTES; i++) {
        VmTrhHeader h = good; ((uint8_t *)&h)[i] ^= 0x80;
        assert(!vm_trh_valid(h, pkg.size()));
    }
    for (uint32_t size : {0u, 63u, (uint32_t)pkg.size() - 1, (uint32_t)pkg.size() + 1}) {
        assert(!vm_trh_valid(good, size));
    }
    auto reject = [&](VmTrhHeader h, VmInstallStatus want) {
        h.headerCrc = 0;
        h.headerCrc = vm_crc32_end(vm_crc32(vm_crc32_begin(), (const uint8_t *)&h, sizeof h));
        const VmInstallResult r = vm_trh_valid(h, VM_TRH_HEADER_BYTES + h.payloadBytes);
        assert(!r && r.status == want);
    };
    {VmTrhHeader h = good; h.magic ^= 1; reject(h, VmInstallStatus::BadMagic);}
    {VmTrhHeader h = good; h.format = 2; reject(h, VmInstallStatus::BadFormat);}
    {VmTrhHeader h = good; h.headerBytes = 32; reject(h, VmInstallStatus::BadHeader);}
    {VmTrhHeader h = good; h.targetBase = 0x60060000u; reject(h, VmInstallStatus::WrongSlot);}
    {VmTrhHeader h = good; h.targetBytes = VM_HOST_SLOT_BYTES * 2; reject(h, VmInstallStatus::WrongSlot);}
    {VmTrhHeader h = good; h.reserved[2] = 1; reject(h, VmInstallStatus::BadHeader);}
    {VmTrhHeader h = good; h.payloadBytes = 0x1000u; reject(h, VmInstallStatus::BadLength);}
    {VmTrhHeader h = good; h.payloadBytes = VM_HOST_SLOT_BYTES + 1; reject(h, VmInstallStatus::BadLength);}
    // A payload stopping inside sector 1 would have its boot words read out of
    // what sector 0 left in the staging buffer, so the floor is both sectors.
    {VmTrhHeader h = good; h.payloadBytes = VM_HOST_SECTOR_BYTES + 4; reject(h, VmInstallStatus::BadLength);}
    {VmTrhHeader h = good; h.payloadBytes = VM_HOST_MIN_PAYLOAD_BYTES - 1; reject(h, VmInstallStatus::BadLength);}
    {VmTrhHeader h = good; h.payloadBytes = VM_HOST_MIN_PAYLOAD_BYTES;
     h.headerCrc = 0;
     h.headerCrc = vm_crc32_end(vm_crc32(vm_crc32_begin(), (const uint8_t *)&h, sizeof h));
     assert(vm_trh_valid(h, VM_TRH_HEADER_BYTES + h.payloadBytes));}

    uint8_t staging[VM_HOST_SECTOR_BYTES];
    auto scan = [&](VmHostCandidate &c, uint32_t failAfter = ~0u) {
        FakeReader r{&pkg}; r.pos = VM_TRH_HEADER_BYTES; r.failAfter = failAfter;
        return vm_host_scan(r, good, staging, c);
    };

    VmHostCandidate candidate;
    assert(scan(candidate));
    assert(candidate.payloadCrc == good.payloadCrc);
    assert(candidate.id.abi == VM_ABI);
    assert(vm_host_slot_valid(candidate.bootWords[0], candidate.bootWords[1], candidate.bootWords[2],
                              candidate.bootWords[3], candidate.bootWords[4]));

    // A payload that is fine except that the minimal image would not enter it
    // must be refused by the scan, before anything is erased.
    {
        std::vector<uint8_t> bad = pkg;
        bad[VM_TRH_HEADER_BYTES + 0x1000] ^= 1;          // vectorMagic
        VmTrhHeader h = good;
        h.payloadCrc = vm_crc32_end(vm_crc32(vm_crc32_begin(), bad.data() + VM_TRH_HEADER_BYTES,
                                             (uint32_t)bad.size() - VM_TRH_HEADER_BYTES));
        VmHostCandidate c; FakeReader r{&bad}; r.pos = VM_TRH_HEADER_BYTES;
        const VmInstallResult got = vm_host_scan(r, h, staging, c);
        assert(!got && got.status == VmInstallStatus::NotBootable);
    }
    {   // and a truncated read is a read error, not a silent short install
        VmHostCandidate c;
        const VmInstallResult got = scan(c, VM_TRH_HEADER_BYTES + 0x2000);
        assert(!got && got.status == VmInstallStatus::ReadError);
    }
    {   // a corrupted payload fails its CRC before the mirrors are consulted
        std::vector<uint8_t> bad = pkg; bad.back() ^= 1;
        VmHostCandidate c; FakeReader r{&bad}; r.pos = VM_TRH_HEADER_BYTES;
        const VmInstallResult got = vm_host_scan(r, good, staging, c);
        assert(!got && got.status == VmInstallStatus::BadPayloadCrc);
    }
    {   // a header claiming an ABI the image does not carry names which disagreed
        VmTrhHeader h = good; h.abi = VM_ABI + 1;
        VmHostCandidate c; FakeReader r{&pkg}; r.pos = VM_TRH_HEADER_BYTES;
        const VmInstallResult got = vm_host_scan(r, h, staging, c);
        assert(!got && got.status == VmInstallStatus::MirrorMismatch);
    }

    const std::vector<uint8_t> installed_image(pkg.begin() + VM_TRH_HEADER_BYTES, pkg.end());
    const std::vector<uint8_t> old_image = incumbent(good.payloadBytes);

    auto fresh = [&] {
        FakeFlash flash;
        memcpy(&flash.cells[0], old_image.data(), old_image.size());
        return flash;
    };
    auto slot_is = [&](const FakeFlash &flash, const std::vector<uint8_t> &want) {
        return memcmp(flash.map(0), want.data(), want.size()) == 0;
    };

    {   // the ordinary case, and the incumbent really was installed first
        FakeFlash flash = fresh();
        assert(vm_host_installed(flash) && slot_is(flash, old_image));
        FakeReader r{&pkg};
        const VmInstallResult done = vm_host_install(flash, r, good, candidate, staging);
        assert(done && done.detail == good.payloadBytes);
        assert(vm_host_installed(flash));
        assert(slot_is(flash, installed_image));
    }

    long total = 0;
    {   // how many operations a whole install performs, so the sweep covers them all
        FakeFlash flash = fresh(); FakeReader r{&pkg};
        assert(vm_host_install(flash, r, good, candidate, staging));
        total = flash.ops;
    }
    assert(total > VM_HOST_SECTORS);

    for (bool fromTail : {false, true}) {
        for (long budget = 0; budget <= total; budget++) {
            FakeFlash flash = fresh();
            flash.budget = budget;
            flash.eraseFromTail = fromTail;
            FakeReader r{&pkg};
            bool cut = false;
            try { vm_host_install(flash, r, good, candidate, staging); }
            catch (const PowerCut &) { cut = true; }
            assert(cut == (budget < total));

            const bool absent = !vm_host_installed(flash);
            const bool untouched = slot_is(flash, old_image);
            const bool complete = slot_is(flash, installed_image);
            assert(absent || untouched || complete);
        }
    }

    {   // an erase that reports failure stops before anything claims to be a host
        FakeFlash flash = fresh(); flash.eraseFailAt = 40; FakeReader r{&pkg};
        const VmInstallResult got = vm_host_install(flash, r, good, candidate, staging);
        assert(!got && got.status == VmInstallStatus::EraseFailed && got.detail == 40);
        assert(!vm_host_installed(flash));
    }
    {   // and a verify failure un-commits rather than leaving a bad host standing
        FakeFlash flash = fresh(); FakeReader r{&pkg};
        VmHostCandidate wrong = candidate; wrong.bodyCrc ^= 1;
        const VmInstallResult got = vm_host_install(flash, r, good, wrong, staging);
        assert(!got && got.status == VmInstallStatus::VerifyFailed);
        assert(!vm_host_installed(flash));
    }

    printf("PASS: TRH1 package header, %u single-bit corruptions, eight malformed-header cases, "
           "scan refusing a non-bootable payload / short read / bad CRC / ABI mirror, a clean install, "
           "and a power cut at each of %ld operations, under a torn erase reaching either half of its "
           "sector, leaving the slot absent, the old host, or the new one\n",
           VM_TRH_HEADER_BYTES, total);
}
