// SPDX-License-Identifier: MIT
// Service bit 20's write job, included inside namespace VmRuntime by both the firmware and the
// transfer test so neither can drift from the other. The platform supplies grantedSlice(),
// which stages one slice, waits until `until` for the client to grant it and then writes it:
// SliceMissed when it was not granted but the client wrote a grant while nothing was armed.
// It also supplies sourceReadable(), which is moduleWindow() on the real address.
enum : int32_t { SliceWaiting = 0, SliceLanded = 1, SliceMissed = 2 };
static int32_t grantedSlice(uint16_t address, const uint8_t *source, uint32_t bytes, uint32_t until);
static bool sourceReadable(const uint8_t *source, uint32_t bytes);

// Whether [address, address + bytes) lies wholly inside one window a module is lent -- its code,
// its data or RAM2 -- and so can be read from DMATransferISR, which would otherwise fault there
// with the C64's bus still driven. RAM2's ends below the host's reserve on profile 1.
static uintptr_t moduleRamLimit = VM_RAM_LIMIT;
static bool moduleWindow(uintptr_t address, uint32_t bytes) {
    const uintptr_t windows[][2] = { { VM_CODE_BASE, VM_CODE_LIMIT }, { VM_DATA_BASE, VM_DATA_LIMIT },
                                     { VM_RAM_BASE, moduleRamLimit } };
    for (const auto &w : windows) if (address >= w[0] && address < w[1] && bytes <= w[1] - address) return true;
    return false;
}
static C64Span jobSpans[VM_C64_SPANS_MAX];
static const uint8_t *jobSources[VM_C64_SPANS_MAX];
static uint32_t jobCount, jobSliceBytes, jobSpan, jobDone, jobTicket, jobWaited;
static bool jobAsking;
static int32_t jobStatus = VM_C64_UNKNOWN;

static uint32_t c64Write(const VmC64Span *spans, uint32_t count, uint32_t sliceBytes, uint32_t flags) {
    if (flags || jobStatus == VM_C64_PENDING || !count || count > VM_C64_SPANS_MAX) return 0;
    for (uint32_t i = 0; i < count; i++) {
        if (!spans[i].source || !sourceReadable(spans[i].source, spans[i].bytes)) return 0;
        jobSpans[i] = { spans[i].address, spans[i].bytes }; jobSources[i] = spans[i].source;
    }
    uint32_t total;
    if (CheckC64Spans(jobSpans, count, sliceBytes, 0x10000, &total) != C64SpansCheck::OK) return 0;
    jobCount = count; jobSliceBytes = sliceBytes; jobSpan = jobDone = 0; jobAsking = false;
    jobTicket = jobTicket == UINT32_MAX ? 1 : jobTicket + 1;
    jobStatus = VM_C64_PENDING;
    return jobTicket;
}

static int32_t c64Status(uint32_t ticket) { return ticket && ticket == jobTicket ? jobStatus : VM_C64_UNKNOWN; }

// Writes granted slices in a turn of its own that starts no slice after 1.5 mS, so a transfer
// cannot starve the module or the client's input; a slice already granted runs to its end, so
// a large slice_bytes, or 0 on a large span, lengthens the turn. A slice is armed only inside
// it, so a grant between turns starts nothing, but it does show the client is granting:
// VM_C64_GRANT_MS counts only armed time since the last grant of either kind, so grants that
// fall into step with the turns slow a job without failing it.
static void c64Step() {
    const uint32_t until = micros() + 1500;
    while (jobStatus == VM_C64_PENDING) {
        if (jobSpan == jobCount) { jobStatus = VM_C64_DONE; return; }
        if (int32_t(micros() - until) >= 0) return;
        uint32_t slice = jobSpans[jobSpan].Len - jobDone;
        if (jobSliceBytes && slice > jobSliceBytes) slice = jobSliceBytes;
        if (!jobAsking) { jobAsking = true; jobWaited = 0; }
        const uint32_t asked = micros();
        const int32_t result = grantedSlice(jobSpans[jobSpan].Addr + jobDone, jobSources[jobSpan] + jobDone, slice, until);
        if (result == SliceMissed) { jobWaited = 0; return; }
        if (result == SliceWaiting) {
            jobWaited += micros() - asked;
            if (jobWaited >= VM_C64_GRANT_MS * 1000u) jobStatus = VM_C64_NO_GRANT;
            return;
        }
        jobAsking = false;
        if (result != SliceLanded) { jobStatus = result; return; }
        jobDone += slice;
        if (jobDone == jobSpans[jobSpan].Len) { jobSpan++; jobDone = 0; }
    }
}
