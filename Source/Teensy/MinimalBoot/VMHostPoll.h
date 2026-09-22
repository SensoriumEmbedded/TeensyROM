// SPDX-License-Identifier: MIT
// The foreground scheduler, shared verbatim by the firmware and the host test.
// VMHost.h supplies the platform and bus services; the test replaces only I/O.
void VMHostPoll() {
    using namespace VmRuntime;
    if (!active) return;
    if (startRequested && !started) {
        started = true; startRequested = false;
        if (failure) { fail(failure); return; }
        EZFlashRAM[0xf5] = 2;
    }
    if (!started || failure || !module) return;
    if (inputPending) {
        VmInput in{ input.buttons, input.display, input.overflow, input.protocol };
        inputPending = false;
        module->input(&in);
        if (failure) return;
    }
    if (pending && EZFlashRAM[0xf6] == sequence) {
        module->ack();
        if (failure) return;
        pending = false; quietRequested = false; EZFlashRAM[0xf5] = 2;
    }
    // Consume an ACK BEFORE pumping: a module may defer input or scene changes
    // while its packet is frozen. Pump-before-ACK followed immediately by
    // packet() starves such input forever when every idle turn emits a packet.
    // An acknowledged packet must also NOT consume the next slice, or a
    // responsive client plus a chatty module starves the module's own clock.
    sliceStarted = micros();
    EZFlashRAM[0xf5] = quietRequested ? 0x12 : 2;
    if (!quietRequested) module->pump();
    if (failure || pending || quietRequested) return;
    if (!module->packet(&packet)) return;   // Nothing to say is not a failure.
    if (failure) return;
    if (packet.length > 228 || packet.reserved || !packet.type) { fail(0x15); return; }
    uint8_t bytes[240];
    sequence = sequence == 255 ? 1 : sequence + 1;
    const unsigned size = encodePacket(bytes);
    for (unsigned i = 0; i < size; i++) EZFlashRAM[i] = bytes[i];
    pending = true;
#if defined(__arm__)
    __asm__ volatile("dmb":::"memory");
#endif
    EZFlashRAM[0xf7] = sequence;
}
