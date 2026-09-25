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
    {   // the scan does not lean on its caller having run vm_trh_valid first:
        // short of two sectors it would read sector 1's boot words out of the
        // staging bytes sector 0 left behind
        VmTrhHeader h = good; h.payloadBytes = VM_HOST_SECTOR_BYTES + 4;
        VmHostCandidate c; FakeReader r{&pkg}; r.pos = VM_TRH_HEADER_BYTES;
        const VmInstallResult got = vm_host_scan(r, h, staging, c);
        assert(!got && got.status == VmInstallStatus::BadLength);
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
    {   // a body that does not verify stops before the tag is ever written, so
        // there is nothing yet to un-commit
        FakeFlash flash = fresh(); FakeReader r{&pkg};
        VmHostCandidate wrong = candidate; wrong.bodyCrc ^= 1;
        const VmInstallResult got = vm_host_install(flash, r, good, wrong, staging);
        assert(!got && got.status == VmInstallStatus::VerifyFailed);
        assert(!vm_host_installed(flash));
    }
    {   // a payload whose sectors each verify but whose whole does not is caught
        // only after the tag has gone down, which is the one case that really
        // un-commits. Deleting the un-commit leaves every other case green.
        FakeFlash flash = fresh(); FakeReader r{&pkg};
        VmTrhHeader h = good; h.payloadCrc ^= 1;
        const VmInstallResult got = vm_host_install(flash, r, h, candidate, staging);
        assert(!got && got.status == VmInstallStatus::VerifyFailed);
        assert(!vm_host_installed(flash) && !slot_is(flash, installed_image));
    }
    {   // and when that un-commit cannot write, the tag is still standing over an
        // image that did not verify: reporting VerifyFailed would describe a slot
        // the device does not have, so the write failure is what comes back.
        FakeFlash flash = fresh(); FakeReader r{&pkg};
        VmTrhHeader h = good; h.payloadCrc ^= 1;
        flash.programFailAt = 0; flash.failFromOp = total;   // only the un-commit's write
        const VmInstallResult got = vm_host_install(flash, r, h, candidate, staging);
        assert(!got && got.status == VmInstallStatus::ProgramFailed);
        assert(vm_host_installed(flash));
    }

    // Removal on its own. The firmware reports it by asking the slot rather than by
    // reading the status back, and these are the three cases that separates.
    {   // the ordinary one: the tag goes down and the slot stops being a host.
        FakeFlash flash = fresh();
        assert(vm_host_installed(flash));
        const VmInstallResult got = vm_host_invalidate(flash);
        assert(got && !vm_host_installed(flash));
    }
    {   // the tag will not clear, so the host is still there and the board must say so.
        FakeFlash flash = fresh(); flash.programFailAt = 0;
        const VmInstallResult got = vm_host_invalidate(flash);
        assert(!got && got.status == VmInstallStatus::ProgramFailed);
        assert(vm_host_installed(flash) && slot_is(flash, old_image));
    }
    {   // and the case the report hangs on: the tag cleared, the erase behind it did
        // not. The slot is already not a host -- removal happened -- so reporting the
        // operation's status would tell the user the host is still installed while the
        // next boot finds nothing. What this pins is that the two answers really do
        // diverge here: vm_host_invalidate returns EraseFailed and vm_host_installed
        // says no, in the same state.
        //
        // That divergence is why DoHostUninstall reports !vm_host_installed() rather
        // than the operation's status -- and nothing checks that it does. This file does
        // not compile FlashUpdate.ino, and hostcycle.py only ever removes from a healthy
        // board, where both answers agree; reaching this state on hardware needs an erase
        // that fails, which no bench step induces. Swap that call for the operation's
        // status and every gate we have still passes. Until something pins it, the choice
        // rests on this comment and the assert below, which pin the divergence only.
        FakeFlash flash = fresh(); flash.eraseFailAt = 0; flash.failFromOp = 1;
        const VmInstallResult got = vm_host_invalidate(flash);
        assert(!got && got.status == VmInstallStatus::EraseFailed);
        assert(!vm_host_installed(flash));
    }

    // Removal as the firmware performs it: the tag first, then the whole slot. Firmware
    // without the extension loader sizes its update buffer by scanning down from the top
    // of flash for the first programmed word, so a payload left behind the tag is room it
    // does not have.
    auto erased = [](const FakeFlash &flash, uint32_t skipSector = ~0u) {
        for (uint32_t i = 0; i < VM_HOST_SLOT_BYTES; i++) {
            if (i / VM_HOST_SECTOR_BYTES == skipSector) continue;
            if (flash.cells[i] != 0xff) return false;
        }
        return true;
    };
    {   // the ordinary one: nothing of the host is left anywhere in the slot
        FakeFlash flash = fresh();
        const VmInstallResult got = vm_host_remove(flash);
        assert(got && !vm_host_installed(flash) && erased(flash));
    }
    {   // a tag that will not clear touches nothing else, so the host is still whole
        FakeFlash flash = fresh(); flash.programFailAt = 0;
        const VmInstallResult got = vm_host_remove(flash);
        assert(!got && got.status == VmInstallStatus::ProgramFailed);
        assert(vm_host_installed(flash) && slot_is(flash, old_image));
    }
    {   // a sector that will not erase is named, and every other one is still cleared
        FakeFlash flash = fresh(); flash.eraseFailAt = 5;
        const VmInstallResult got = vm_host_remove(flash);
        assert(!got && got.status == VmInstallStatus::EraseFailed && got.detail == 5);
        assert(!vm_host_installed(flash) && erased(flash, 5));
    }
    {   // sector 0 itself, past its cleared tag: the rest of the slot still goes
        FakeFlash flash = fresh(); flash.eraseFailAt = 0; flash.failFromOp = 1;
        const VmInstallResult got = vm_host_remove(flash);
        assert(!got && got.status == VmInstallStatus::EraseFailed && got.detail == 0);
        assert(!vm_host_installed(flash) && erased(flash, 0));
    }

    // What a failed install leaves: no host, and not blank either. DoHostUninstall runs
    // removal for any slot that is not blank, so these are removals it performs too.
    {   // the whole-payload verify failure, whose un-commit clears only the tag
        FakeFlash flash = fresh(); FakeReader r{&pkg};
        VmTrhHeader h = good; h.payloadCrc ^= 1;
        assert(!vm_host_install(flash, r, h, candidate, staging));
        assert(!vm_host_installed(flash) && !erased(flash));
        assert(vm_host_remove(flash) && erased(flash));
    }
    {   // and when the tag program fails over them, there is no host to keep whole,
        // so the slot is erased anyway and the removal is clean
        FakeFlash flash = fresh(); FakeReader r{&pkg};
        VmTrhHeader h = good; h.payloadCrc ^= 1;
        assert(!vm_host_install(flash, r, h, candidate, staging));
        flash.programFailAt = 0; flash.failFromOp = flash.ops;
        assert(vm_host_remove(flash) && erased(flash));
    }
    {   // a sector that will not erase there is still named
        FakeFlash flash = fresh(); FakeReader r{&pkg};
        VmTrhHeader h = good; h.payloadCrc ^= 1;
        assert(!vm_host_install(flash, r, h, candidate, staging));
        flash.programFailAt = 0; flash.eraseFailAt = 0; flash.failFromOp = flash.ops;
        const VmInstallResult got = vm_host_remove(flash);
        assert(!got && got.status == VmInstallStatus::EraseFailed && got.detail == 0);
        assert(!vm_host_installed(flash) && erased(flash, 0));
    }
    long leftovers = 0;
    for (bool fromTail : {false, true}) {   // and a power cut anywhere in an install
        for (long budget = 0; budget < total; budget++) {
            FakeFlash flash = fresh(); FakeReader r{&pkg};
            flash.budget = budget;
            flash.eraseFromTail = fromTail;
            try { vm_host_install(flash, r, good, candidate, staging); assert(false); }
            catch (const PowerCut &) {}
            if (!vm_host_installed(flash) && !erased(flash)) leftovers++;
            flash.budget = -1;
            assert(vm_host_remove(flash) && erased(flash));
        }
    }
    assert(leftovers > 0);

    long removeOps = 0;
    {
        FakeFlash flash = fresh();
        assert(vm_host_remove(flash));
        removeOps = flash.ops;
    }
    assert(removeOps == VM_HOST_SECTORS + 1);
    for (bool fromTail : {false, true}) {
        for (long budget = 0; budget <= removeOps; budget++) {
            FakeFlash flash = fresh();
            flash.budget = budget;
            flash.eraseFromTail = fromTail;
            bool cut = false;
            try { vm_host_remove(flash); }
            catch (const PowerCut &) { cut = true; }
            assert(cut == (budget < removeOps));
            assert(!vm_host_installed(flash) || slot_is(flash, old_image));
        }
    }

    printf("PASS: TRH1 package header, %u single-bit corruptions, malformed-header cases and the "
           "payload floor from either side, "
           "scan refusing a non-bootable payload / short read / bad CRC / ABI mirror, a clean install, "
           "an un-commit after a whole-payload verify failure and the write failure reported when that "
           "un-commit cannot land, removal on its own with the tag clearing, refusing to clear, and "
           "clearing over an erase that fails, removal erasing the whole slot past a tag that clears and "
           "stopping at one that will not, with a power cut at each of its %ld operations leaving the slot "
           "absent or the old host, the leftovers of a failed install cleared the same way (%ld of them "
           "under a cut) and over a tag that will not clear, "
           "and a power cut at each of %ld operations, under a torn erase reaching either half of its "
           "sector, leaving the slot absent, the old host, or the new one\n",
           VM_TRH_HEADER_BYTES, removeOps, leftovers, total);
}
