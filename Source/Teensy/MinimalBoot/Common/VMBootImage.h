// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "VMHostABI.h"

// The extension host's flash slot, as the ordinary images see it. The geometry
// and the validity predicate are the published host contract (VMHostABI.h);
// this adds the reads against real flash, the one way to render a descriptor's
// name for a message, and the buffer that stands in for the slot on host
// conformance builds.
//
// The ordinary images keep their upstream addresses. Only the extension image
// uses the module-compatible RAM map.
namespace VmBootImage {
static constexpr uint32_t base = VM_HOST_SLOT_BASE;
static constexpr uint32_t limit = VM_HOST_SLOT_LIMIT;

static constexpr uint32_t idOffset = VM_HOST_ID_OFFSET;

#if defined(__arm__)
static inline const uint8_t *window() { return reinterpret_cast<const uint8_t *>(base); }
#else
// Host conformance builds have no flash behind `base`, so the slot they inspect
// is an ordinary buffer, which install() below fills. Sized to the whole slot,
// so it can stand in for the region an installer writes.
alignas(uint32_t) inline uint8_t hostWindow[VM_HOST_SLOT_BYTES];
static inline const uint8_t *window() { return hostWindow; }
#endif

static inline bool valid(uint32_t flashMagic, uint32_t vectorMagic,
                         uint32_t entry, uint32_t bootBase, uint32_t imageBytes) {
    return vm_host_slot_valid(flashMagic, vectorMagic, entry, bootBase, imageBytes);
}
static inline bool installed() {
    const auto flash = reinterpret_cast<const volatile uint32_t *>(window());
    // BootData follows the eight-word image vector table.
    return valid(flash[0], flash[0x1000 / 4], flash[0x1004 / 4],
                 flash[0x1020 / 4], flash[0x1024 / 4]);
}

// False only when the slot holds no descriptor at all: a host built before it
// existed reads as erased flash. That is "cannot say", not "provides nothing"
// -- see tryLaunch, which proceeds on false and judges the rest.
static inline bool identity(VmHostId &out) {
    VmHostId id{};
    memcpy(&id, window() + idOffset, sizeof id);
    if (id.magic != VM_HOSTID_MAGIC) return false;
    out = id;
    return true;
}

// The name a host message shows, for every message that shows one -- the install and
// removal notices, the menu's host line and both launch refusals all come through here,
// because copies of it are how the buffer sizes drifted apart in the first place, and
// because a filter is only worth having where nothing can go round it.
//
// Takes the array rather than a pointer so that rewriting this as a `const char *`
// fails to compile: `sizeof` a pointer is 4, which would quietly size nameBytes below
// from the field alone and restore the char[13] that a strcpy of the placeholder used
// to overrun by three.
static constexpr char noDescriptor[] = "(no descriptor)";
static constexpr char unnamedHost[] = "(unnamed)";
template<unsigned N> static constexpr unsigned literalBytes(const char (&)[N]) { return N; }
static constexpr unsigned largest(unsigned a, unsigned b) { return a > b ? a : b; }

// Sized for whichever source is longest, because they are not the same length:
// VmHostId::name is a fixed 12 bytes and need not be terminated, while the placeholders
// are 15 and 9. A buffer sized from the field alone -- the obvious `sizeof id.name + 1`
// -- is three bytes short of the longest.
//
// Every source that exists goes through largest(), but nothing makes the next one: a
// placeholder added to displayName and not added here leaves the constant behind, and
// the only thing that catches it is the screen. It truncates rather than overruns --
// displayName writes through snprintf, which is bounded by the caller's size -- so the
// failure is a name that comes up short, not the char[13] strcpy this replaced.
static constexpr unsigned nameBytes =
    largest(largest(sizeof(VmHostId::name) + 1, literalBytes(noDescriptor)),
            literalBytes(unnamedHost));

// The descriptor's twelve bytes are third-party data: vm_host_scan checks the magic, the
// ABI, the services, both CRCs and the boot words, and identity() checks only the magic,
// so nothing upstream constrains their content. They are printed to a C64, which executes
// control codes rather than drawing them -- and the removal notice they appear in is the
// one carrying "do not power off" across a 45-second erase, so a name holding $93 (clear
// screen) or $0d (return) takes that warning off the screen.
//
// The control ranges are $00-$1f and $80-$9f. IOH_Swiftlink.c settles it: all nineteen
// control codes it names fall inside them -- return $0d, the charset pair $0e/$8e,
// reverse $12/$92, clear $93, cursor $91 and its twelve colours -- and the only two it
// names outside them are glyphs, space $20 and horizBar $60. So everything outside the
// two ranges draws, including the graphics blocks a host author may have picked on
// purpose. Judging this by isprint() instead would have cut $60-$7f and $a0-$ff, which
// are those blocks.
static inline bool nameByteDraws(unsigned char c) {
    return !(c < 0x20 || (c >= 0x80 && c <= 0x9f));
}

// $20 and $a0 are the two blanks, space and shifted space. A name of nothing but these
// passes nameByteDraws byte for byte and still reaches the screen as an empty row, which
// during an erase is the same failure as a cleared one: nothing left to report.
static inline bool nameByteBlank(unsigned char c) { return c == 0x20 || c == 0xa0; }

// $3f, which draws in every charset. Substituted for a byte that would not draw.
static constexpr char nameSubstitute = '?';

// `bytes` is the caller's buffer, which is nameBytes wide if it wants any source whole;
// every write is bounded by it, so a later edit to any source truncates rather than
// running off the end. A byte that would not draw is substituted rather than dropped, so
// a name that is entirely control codes still renders twelve visible characters someone
// can read back and report -- dropping them would leave a blank where the host's identity
// belongs, which is the failure this is here to prevent. The loop stops at the field's
// first NUL and reads no more than its twelve bytes, which is what handles a name filling
// the field with no terminator. A null `id` is identity() saying no: a host built before
// the descriptor existed, whose name cannot be read rather than being blank.
static inline void displayName(char *out, size_t bytes, const VmHostId *id) {
    if (!bytes) return;
    if (!id) { snprintf(out, bytes, "%s", noDescriptor); return; }

    size_t n = 0;
    bool drawn = false;
    for (size_t i = 0; i < sizeof id->name && n + 1 < bytes; i++) {
        const unsigned char c = (unsigned char)id->name[i];
        if (!c) break;
        out[n++] = nameByteDraws(c) ? (char)c : nameSubstitute;
        if (!nameByteBlank(c)) drawn = true;
    }
    out[n] = 0;
    if (!drawn) snprintf(out, bytes, "%s", unnamedHost);
}

#if !defined(__arm__)
// Writes what installed() and identity() read, so the layout stays beside the
// code that reads it rather than in each test.
inline void installWithoutDescriptor() {
    const uint32_t words[] = { 0x42464346u, 0u, 0u, 0u };
    memset(hostWindow, 0, sizeof hostWindow);
    memcpy(hostWindow, words, sizeof words);
    const uint32_t vector = 0x432000d1u, entry = base + 0x1001u, boot = base, bytes = 0x2000u;
    memcpy(hostWindow + 0x1000, &vector, 4);
    memcpy(hostWindow + 0x1004, &entry, 4);
    memcpy(hostWindow + 0x1020, &boot, 4);
    memcpy(hostWindow + 0x1024, &bytes, 4);
    memset(hostWindow + idOffset, 0xff, sizeof(VmHostId));
}

inline void install(uint32_t services, uint32_t abi = VM_ABI, const char *name = "TeensyROM") {
    installWithoutDescriptor();
    VmHostId id = { VM_HOSTID_MAGIC, abi, services, sizeof(VmHost), {}, 0 };
    snprintf(id.name, sizeof id.name, "%s", name);
    memcpy(hostWindow + idOffset, &id, sizeof id);
}
#endif
}
