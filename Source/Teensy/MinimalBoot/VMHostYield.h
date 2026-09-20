// SPDX-License-Identifier: MIT
// Scheduling policy, included inside namespace VmRuntime by both the firmware
// and the scheduler test so neither can drift from the other.
//
// A module is never preempted. It hands the foreground back when the client
// needs attention or when its slice is up; a module that ignores this makes
// the machine unresponsive.
static bool shouldYield() {
    return inputPending || quietRequested || (pending && EZFlashRAM[0xf6] == sequence) ||
           uint32_t(micros() - sliceStarted) >= 1500;
}
