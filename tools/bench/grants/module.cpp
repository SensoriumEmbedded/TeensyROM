// SPDX-License-Identifier: MIT
//
// The module half of tools/bench/grants: writes 4 KiB to $4000 over and over through service
// bit 20 for 1.5 seconds, a slice per grant, and exits with how that went.
//
// Its content file holds the slice size, low byte first, and whether the client should grant
// (1) or never grant (0). The exit status is jobs << 16 | ticks lost, or, when a write
// failed, 0x80000000 | the c64_status it failed with, or 0x800000EE when what the C64 holds
// at the end is not what was written. So that a run that stops still comes back to say where,
// a job pending over a second exits 0xE0000000 | jobs, and a question left unanswered for two
// seconds 0xD0000000 | jobs << 8 | its state.
//
// It asks its client for the ticks lost so far before and after, and then for a sum over the
// bytes written, by packet; see client.a.
#include "../../../vm/abi/vm_abi.h"

namespace {

constexpr uint16_t kAddress = 0x4000, kBytes = 4096;
constexpr uint32_t kRunMicros = 1500000, kJobMicros = 1000000, kAnswerMicros = 2000000;

const VmHostC64Dma *host;
uint8_t pattern[kBytes];
uint16_t sliceBytes;
uint8_t grant = 1;
enum : uint8_t { Ask1, Wait1, Run, Ask2, Wait2, Ask3, Wait3 } state;
bool awaitingAck;
uint16_t lostBefore, lostAfter;
uint8_t sum1, sum2;
uint32_t runStarted, ticket, jobs, jobStarted, asked;

void finish(uint32_t status) { host->base.exit_to_menu(status); }
uint32_t now() { return host->base.base.micros_now(); }

void input(const VmInput *in) {
    const uint16_t lost = uint16_t(in->buttons | in->display << 8);
    if (state == Wait1 && in->overflow == 1) { lostBefore = lost; runStarted = now(); state = Run; }
    else if (state == Wait2 && in->overflow == 2) { lostAfter = lost; state = Ask3; }
    else if (state == Wait3 && in->overflow == 3)
        finish(in->buttons == sum1 && in->display == sum2 ? jobs << 16 | uint16_t(lostAfter - lostBefore) : 0x800000EEu);
}

void pump() {
    if ((state == Wait1 || state == Wait2 || state == Wait3) && now() - asked >= kAnswerMicros)
        finish(0xD0000000u | (jobs & 0xFFFF) << 8 | state);
    if (state != Run) return;
    const VmC64Span span = { pattern, kAddress, kBytes };
    if (ticket) {
        const int32_t status = host->c64_status(ticket);
        if (status == VM_C64_PENDING) {
            if (now() - jobStarted >= kJobMicros) finish(0xE0000000u | jobs);
            return;
        }
        if (status != VM_C64_DONE) finish(0x80000000u | uint8_t(status));
        ticket = 0; jobs++;
    }
    if (host->base.base.micros_now() - runStarted >= kRunMicros) { state = Ask2; return; }
    ticket = host->c64_write(&span, 1, sliceBytes, 0);
    jobStarted = now();
    if (!ticket) finish(0x80000000u);
}

bool packet(VmPacket *out) {
    if (awaitingAck || (state != Ask1 && state != Ask2 && state != Ask3)) return false;
    *out = {};
    out->type = 1;
    out->payload[0] = state == Ask1 ? 1 : state == Ask2 ? 2 : 3;
    out->payload[1] = state == Ask1 ? grant : 0;
    out->payload[2] = uint8_t(sliceBytes);
    out->payload[3] = uint8_t(sliceBytes >> 8);
    out->length = 4;
    state = state == Ask1 ? Wait1 : state == Ask2 ? Wait2 : Wait3;
    asked = now();
    awaitingAck = true;
    return true;
}

void ack() { awaitingAck = false; }

const VmModule module = { VM_ABI, sizeof(VmModule), input, pump, packet, ack };

}  // namespace

VM_MODULE_ENTRY const VmModule *vm_entry(const VmHost *h) {
    if (!h || h->abi != VM_ABI || h->bytes < VM_HOST_C64_DMA_BYTES) return nullptr;
    if ((h->services & (VM_SERVICE_C64_DMA | VM_SERVICE_EXIT)) != (VM_SERVICE_C64_DMA | VM_SERVICE_EXIT)) return nullptr;
    host = reinterpret_cast<const VmHostC64Dma *>(h);
    VmFileInfo info{};
    uint8_t config[3] = {};
    const uint32_t file = h->open(h->content_path, &info);
    if (file) { h->read(file, 0, config, sizeof config); h->close(file); }
    sliceBytes = uint16_t(config[0] | config[1] << 8);
    grant = config[2];
    // Different at every slice size, so a run whose writes never landed cannot pass for one
    // that did.
    for (uint32_t n = 0; n < kBytes; n++) {
        pattern[n] = uint8_t(n ^ n >> 8 ^ sliceBytes ^ grant << 7);
        sum1 = uint8_t(sum1 + pattern[n]); sum2 = uint8_t(sum2 + sum1);
    }
    return &module;
}
