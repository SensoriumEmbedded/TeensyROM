// SPDX-License-Identifier: MIT
// VmFail::report() against a C64 that never answers. On hardware each attempt
// costs SendMsgSerialStringBuf's three-second wait and leaves pending() set, so
// how many attempts there can be is what is asserted here. The record's RAM2
// cache handling needs the chip and is not exercised.
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define FLASHMEM
static std::vector<std::string> toC64, toSerial;

static void collect(std::vector<std::string> &into, const char *format, va_list ap) {
    char line[256];
    vsnprintf(line, sizeof line, format, ap);
    into.push_back(line);
}

void SendMsgPrintfln(const char *format, ...) {
    va_list ap; va_start(ap, format); collect(toC64, format, ap); va_end(ap);
}

static struct UsbSerial {
    void printf(const char *format, ...) {
        va_list ap; va_start(ap, format); collect(toSerial, format, ap); va_end(ap);
    }
} Serial;

static void arm_dcache_flush_delete(void *, size_t) {}
static void arm_dcache_delete(void *, size_t) {}

#include "../../Source/Teensy/MinimalBoot/Common/VMFail.h"

// What IOH_TeensyROM.c's polling handler does: report, and clear only when the
// C64 answered rsContinue inside the message wait.
static void poll(bool c64Answers) {
    if (!VmFail::pending()) return;
    VmFail::report();
    if (c64Answers) VmFail::clear();
}

// Three attempts is three times SendMsgSerialStringBuf's three-second wait.
static_assert(VmFail::reportAttemptLimit == 3, "the menu stalls once per attempt");

int main() {
    (void)&VmFail::capture;   // reads the fixed RAM2 address; needs the chip

    VmFail::captured = VmFail::Record{ VmFail::Magic, VmFail::ModuleLoad, 0x11, 0, {} };
    VmFail::captureValid = VmFail::captureHeld = true;

    // A C64 that never reaches its wait loop gets the limit and no more, and
    // the reason ends up on the Teensy's own serial instead of nowhere.
    for (unsigned i = 0; i < 10; i++) poll(false);
    assert(toC64.size() == VmFail::reportAttemptLimit);
    assert(!VmFail::pending());
    assert(toSerial.size() == 1 && toSerial[0].find("module refused") != std::string::npos);
    assert(toC64[0] == "Extension: module refused ($20/$11)");

    // A C64 that does answer is told once, and never again.
    toC64.clear(); toSerial.clear();
    VmFail::reportAttempts = 0;
    VmFail::captureHeld = true;
    for (unsigned i = 0; i < 10; i++) poll(true);
    assert(toC64.size() == 1 && toSerial.empty() && !VmFail::pending());

    // A reason with no detail keeps the shorter form.
    toC64.clear();
    VmFail::captured.detail = 0;
    VmFail::reportAttempts = 0;
    VmFail::captureHeld = true;
    poll(true);
    assert(toC64.size() == 1 && toC64[0] == "Extension: module refused ($20)");

    // printBoot() is the serial half of the same record, and always speaks.
    toSerial.clear();
    VmFail::printBoot();
    assert(toSerial.size() == 1 && toSerial[0].find("module refused") != std::string::npos);

    puts("PASS: extension failure reporting; bounded retries when the C64 never reads, "
         "one message when it does, both message forms, and the serial line");
}
