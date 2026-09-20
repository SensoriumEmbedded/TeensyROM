// SPDX-License-Identifier: MIT
//
// Reference extension module: the smallest thing that proves the loader works.
//
// It validates the host, uses the file service to count the entries in its own
// package directory, and publishes three lines of text for its C64 client to
// put on screen. Pressing a button recolours them.
//
// Packet type 1 below is a private agreement between THIS module and
// Source/C64/VMHello -- the host never looks inside a payload, it only frames
// and delivers it. Define whatever protocol suits your extension; you do not
// need a firmware change to do it.
//
// Freestanding: no libc, no static constructors, no heap. Everything the
// module owns lives in .data/.bss inside its own 192 KiB DTCM window.
#include "../abi/vm_abi.h"

namespace {

constexpr uint8_t kRows = 4, kColumns = 40;
constexpr uint8_t kPacketText = 1;

const VmHost *host;
uint8_t screen[kRows][kColumns];   // PETSCII screen codes, space-filled
uint8_t colour = 5;                // 5 = green
uint8_t nextRow;                   // next line to publish
bool awaitingAck;
bool repaint;

// Screen codes, not PETSCII: 'A'..'Z' are 1..26 and space is 32.
uint8_t screenCode(char c) {
    if (c >= 'A' && c <= 'Z') return uint8_t(c - 64);
    if (c >= 'a' && c <= 'z') return uint8_t(c - 96);
    return uint8_t(c);
}

// Both writers return the column after what they wrote, so callers can flow
// text and numbers along a line without counting characters by hand. Writing
// past the right-hand edge is clipped, never wrapped.
uint8_t putText(uint8_t row, uint8_t column, const char *text) {
    while (*text && column < kColumns) screen[row][column++] = screenCode(*text++);
    return column;
}

uint8_t putNumber(uint8_t row, uint8_t column, uint32_t value) {
    char digits[11];
    uint8_t n = 0;
    do { digits[n++] = char('0' + value % 10); value /= 10; } while (value && n < sizeof digits);
    while (n && column < kColumns) screen[row][column++] = screenCode(digits[--n]);
    return column;
}

// Counts the entries in package_root, which is also a live check that the file
// service works before we claim success on screen.
uint32_t countPackageEntries() {
    VmFileInfo info{};
    const uint32_t directory = host->open(host->package_root, &info);
    if (!directory || !info.directory) return 0;
    uint32_t entries = 0;
    while (host->next(directory, &info) == 1) ++entries;
    host->close(directory);
    return entries;
}

void input(const VmInput *in) {
    // Any button cycles the colour and asks for a full repaint.
    static uint8_t previous;
    if (in->buttons && in->buttons != previous) {
        colour = uint8_t(colour == 15 ? 1 : colour + 1);
        repaint = true;
    }
    previous = in->buttons;
}

void pump() {
    // Nothing to simulate. A real extension advances its guest here, in bounded
    // slices, calling host->should_yield() to stay responsive.
}

bool packet(VmPacket *out) {
    if (awaitingAck) return false;
    if (repaint) { nextRow = 0; repaint = false; }
    // Nothing left to publish. Saying nothing is normal and costs the host
    // nothing; do not invent filler packets to keep the link busy.
    if (nextRow >= kRows) return false;
    *out = {};
    out->type = kPacketText;
    out->payload[0] = nextRow;
    out->payload[1] = colour;
    for (uint8_t i = 0; i < kColumns; ++i) out->payload[2 + i] = screen[nextRow][i];
    out->length = uint8_t(2 + kColumns);
    ++nextRow;
    awaitingAck = true;
    return true;
}

void ack() { awaitingAck = false; }

const VmModule module = { VM_ABI, sizeof(VmModule), input, pump, packet, ack };

}  // namespace

VM_MODULE_ENTRY const VmModule *vm_entry(const VmHost *h) {
    // Reject anything that is not at least a base-profile ABI 2 host. Checking
    // `bytes >= VM_HOST_BASE_BYTES` rather than `== sizeof(VmHost)` is what lets
    // this module also run on a host that has appended its own callbacks.
    if (!h || h->abi != VM_ABI || h->bytes < VM_HOST_BASE_BYTES) return nullptr;
    if ((h->services & VM_SERVICES) != VM_SERVICES) return nullptr;
    host = h;

    for (uint8_t row = 0; row < kRows; ++row)
        for (uint8_t column = 0; column < kColumns; ++column) screen[row][column] = 32;

    putText(0, 0, "HELLO WORLD FROM TEENSYROM");
    putText(1, 0, h->package_root);
    uint8_t column = putText(2, 0, "WORKSPACE ");
    column = putNumber(2, column, h->workspace_bytes);
    column = putText(2, column, " GUEST ");
    putNumber(2, column, h->guest_ram_bytes);
    column = putText(3, 0, "PACKAGE FILES ");
    putNumber(3, column, countPackageEntries());

    // Touch both ends of each lent region: a silent overlap in the memory map
    // is far easier to find here than in an extension ten thousand lines long.
    if (h->workspace_bytes) { h->workspace[0] = 0; h->workspace[h->workspace_bytes - 1] = 0; }
    if (h->guest_ram_bytes) { h->guest_ram[0] = 0; h->guest_ram[h->guest_ram_bytes - 1] = 0; }
    (void)h->micros_now();
    return &module;
}
