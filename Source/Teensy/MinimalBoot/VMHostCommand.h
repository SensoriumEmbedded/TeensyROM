// SPDX-License-Identifier: MIT
// The $DFF4 command register, included inside namespace VmRuntime by both the
// firmware and the scheduler test so neither can drift from the other.
//
// 1 starts the module, and resumes it once it is running; 4 asks it to stop
// running; 3 offers the input record staged in $DFF8..$DFFA and $DFFD..$DFFF.
// Quiet is lifted by an ACK or by 1, because a client that goes quiet with no
// packet outstanding has no ACK left to send.
static void commandWrite(uint8_t value) {
    if (value == 1) { if (started) quietRequested = false; else startRequested = true; }
    if (value == 4) quietRequested = true;
    if (value != 3 || inputPending || !EZFlashRAM[0xfe] || EZFlashRAM[0xfe] == EZFlashRAM[0xfc]) return;
    const uint8_t expected = 0xa5 ^ EZFlashRAM[0xf8] ^ EZFlashRAM[0xf9] ^ EZFlashRAM[0xfa] ^
                             EZFlashRAM[0xfd] ^ EZFlashRAM[0xfe];
    if (expected != EZFlashRAM[0xff]) return;
    input.buttons = EZFlashRAM[0xf8]; input.display = EZFlashRAM[0xf9];
    input.overflow = EZFlashRAM[0xfa]; input.protocol = EZFlashRAM[0xfd];
#if defined(__arm__)
    __asm__ volatile("dmb":::"memory");
#endif
    inputPending = true; EZFlashRAM[0xfc] = EZFlashRAM[0xfe];
}
