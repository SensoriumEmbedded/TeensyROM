// SPDX-License-Identifier: MIT
// End-to-end conformance run for the reference module: the real module source,
// against the real base-profile host, with no hardware and no ARM toolchain.
#include "native_host.h"
#include "../hello/hello.cpp"
#include <cassert>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static char fromScreenCode(uint8_t code) { return code >= 1 && code <= 26 ? char('A' + code - 1) : char(code); }

static std::string textOf(const VmPacket &packet) {
    std::string out;
    for (uint8_t i = 2; i < packet.length; ++i) out += fromScreenCode(packet.payload[i]);
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

// A host the module must not take the exit on, checked the same way wherever it
// comes from. Two witnesses: exitCalls for the callback, and the banner for what
// vm_entry resolved -- a module that resolved an exit and never called it would
// satisfy the counter on its own.
//
// input() keeps its edge state in a static, so the release goes first. On the
// board that state starts clean because vm_load_payload() zeroes .bss; a second
// vm_entry in one native process does not.
static void assertDeclinesExit(const VmModule *loaded, VmPacket &packet) {
    const VmInput released{ 0, 0, 0, 0x81 }, up{ 2, 0, 0, 0x81 }, press{ 1, 0, 0, 0x81 };
    assert(loaded);
    while (loaded->packet(&packet)) loaded->ack();   // nothing outstanding
    loaded->input(&released);
    loaded->input(&up);
    assert(!loaded->packet(&packet));   // not a fault, and not a repaint
    assert(VmNativeHost::exitCalls == 0);
    assert(!VmNativeHost::lastFailure);
    // Fire repaints from row zero, which is where the banner lives.
    loaded->input(&released);
    loaded->input(&press);
    assert(loaded->packet(&packet) && packet.payload[0] == 0);
    assert(textOf(packet) == "HELLO WORLD FROM TEENSYROM");
    assert(VmNativeHost::exitCalls == 0 && !VmNativeHost::lastFailure);
    loaded->ack();
}

int main(int argc, char **argv) {
    assert(argc == 2);
    const fs::path sandbox = argv[1];
    const fs::path package = sandbox / "VMS/HELLO";
    fs::create_directories(package);
    for (const char *name : { "manifest.vmi", "engine.mvm", "client.crt" })
        std::ofstream(package / name, std::ios::binary) << name;

    std::vector<uint8_t> workspace(64 * 1024), guest(VM_RAM_BYTES);
    const std::string root = package.string();

    // A module must refuse a host it does not understand, and must refuse it
    // without touching lent memory. Each of these is a real rejection path.
    assert(vm_entry(nullptr) == nullptr);
    {
        VmHost bad = VmNativeHost::make(root, "", workspace.data(), workspace.size(), guest.data(), guest.size());
        bad.abi = VM_ABI + 1;
        assert(vm_entry(&bad) == nullptr);
    }
    {
        VmHost bad = VmNativeHost::make(root, "", workspace.data(), workspace.size(), guest.data(), guest.size());
        bad.bytes = VM_HOST_BASE_BYTES - 1;
        assert(vm_entry(&bad) == nullptr);
    }
    {
        VmHost bad = VmNativeHost::make(root, "", workspace.data(), workspace.size(), guest.data(), guest.size());
        bad.services = VM_SERVICE_FILES | VM_SERVICE_CLOCK;  // no packets/write/guest RAM
        assert(vm_entry(&bad) == nullptr);
    }

    VmHost host = VmNativeHost::make(root, "", workspace.data(), uint32_t(workspace.size()), guest.data(), uint32_t(guest.size()));
    const VmModule *loaded = vm_entry(&host);
    assert(loaded && loaded->abi == VM_ABI && loaded->bytes == sizeof(VmModule));
    assert(loaded->input && loaded->pump && loaded->packet && loaded->ack);
    assert(!VmNativeHost::lastFailure);

    // Three lines, published one at a time, each held until the client ACKs.
    VmPacket packet{};
    std::vector<std::string> lines;
    for (int row = 0; row < 4; ++row) {
        assert(loaded->packet(&packet));
        assert(packet.type == 1 && packet.length == 42 && !packet.reserved);
        assert(packet.payload[0] == row && packet.payload[1] == 5);
        // A published packet is frozen until ACK: no second packet, and pump()
        // must not disturb it.
        VmPacket again{};
        assert(!loaded->packet(&again));
        loaded->pump();
        assert(!loaded->packet(&again));
        lines.push_back(textOf(packet));
        loaded->ack();
    }
    // Nothing further to say, and saying nothing is not a failure.
    assert(!loaded->packet(&packet));
    loaded->pump();
    assert(!loaded->packet(&packet));

    // The unshifted C64 charset has no lowercase, so a round trip through
    // screen codes folds the path to upper case and clips it at 40 columns.
    std::string folded = root;
    for (char &c : folded) if (c >= 'a' && c <= 'z') c = char(c - 32);
    if (folded.size() > 40) folded.resize(40);
    assert(lines[0] == "HELLO WORLD FROM TEENSYROM");
    assert(lines[1] == folded);
    // Assert the rendered line, not just the numbers in it. The sizes come from
    // the ABI header so that shrinking the guest arena shows up here as a
    // changed expectation rather than a stale literal.
    assert(workspace.size() == 65536 && guest.size() == VM_RAM_BYTES);
    assert(lines[2] == "WORKSPACE " + std::to_string(workspace.size()) +
                       " GUEST " + std::to_string(guest.size()));
    // The file service really walked the package directory.
    assert(lines[3] == "PACKAGE FILES 3");

    // Input recolours and forces a repaint from row zero.
    VmInput press{ 1, 0, 0, 0x81 }, up{ 2, 0, 0, 0x81 }, released{ 0, 0, 0, 0x81 };
    loaded->input(&press);
    for (int row = 0; row < 4; ++row) {
        assert(loaded->packet(&packet));
        assert(packet.payload[0] == row && packet.payload[1] == 6);
        loaded->ack();
    }
    assert(!loaded->packet(&packet));
    // A held button is not a new press.
    loaded->input(&press);
    assert(!loaded->packet(&packet));
    assert(!VmNativeHost::lastFailure);

    // Up on a host that lends no exit does nothing at all. That is the fallback
    // half of "capability, then fallback", and it is the state make() builds:
    // neither the bit nor the tail.
    assert(host.bytes == sizeof(VmHost) && !(host.services & VM_SERVICE_EXIT));
    assertDeclinesExit(loaded, packet);

    // On a host that does lend one, up takes it.
    {
        VmHostExit exitHost = VmNativeHost::makeWithExit(root, "", workspace.data(),
            uint32_t(workspace.size()), guest.data(), uint32_t(guest.size()));
        const VmModule *withExit = vm_entry(&exitHost.base);
        assert(withExit && !VmNativeHost::lastFailure);
        assert(VmNativeHost::exitCalls == 0);
        withExit->input(&released);          // establish the edge from a clean state
        withExit->input(&up);
        assert(VmNativeHost::exitCalls == 1 && VmNativeHost::lastExitStatus == 0);
        // Held, not pressed again: one exit per edge, so a module that survived
        // the call does not ask twice.
        withExit->input(&up);
        assert(VmNativeHost::exitCalls == 1);
        // Fire still recolours on the same host: taking the exit did not replace
        // the rest of the module's input handling. Row zero carries the banner
        // vm_entry writes when it resolved one.
        withExit->input(&released);
        withExit->input(&press);
        assert(withExit->packet(&packet) && packet.payload[0] == 0);
        assert(textOf(packet) == "HELLO WORLD  JOY2 UP QUITS");
        assert(VmNativeHost::exitCalls == 1);
        withExit->ack();
    }

    // `bytes` and the service bit are independent (vm/abi/README.md, "Tail
    // extensions"), and a host holding one without the other is the only thing
    // that tells a module checking both from one checking either alone. With
    // them both present or both absent, all three behave identically.
    {
        // Grown for a tail extension, lending none of it. exit_to_menu stays
        // wired so a module that skipped the service-bit check is counted here
        // rather than jumping somewhere undefined.
        VmHostExit grown = VmNativeHost::makeWithExit(root, "", workspace.data(),
            uint32_t(workspace.size()), guest.data(), uint32_t(guest.size()));
        grown.base.services &= ~uint32_t(VM_SERVICE_EXIT);
        assert(grown.base.bytes >= VM_HOST_EXIT_BYTES);
        assertDeclinesExit(vm_entry(&grown.base), packet);
    }
    {
        // The bit, published by a host whose struct stops at the base profile:
        // it serves the exit some other way, and the tail is not this module's
        // to read. The callback sits behind `bytes` for the same reason as
        // above -- a module reading past it is counted, not left undefined.
        VmHostExit elsewhere = VmNativeHost::makeWithExit(root, "", workspace.data(),
            uint32_t(workspace.size()), guest.data(), uint32_t(guest.size()));
        elsewhere.base.bytes = VM_HOST_BASE_BYTES;
        assert(elsewhere.base.services & VM_SERVICE_EXIT);
        assertDeclinesExit(vm_entry(&elsewhere.base), packet);
    }

    printf("PASS: hello module loads on a base-profile host, rejects three bad hosts, "
           "publishes %zu ACK-gated lines, walks its package directory, repaints on input, takes "
           "the exit service on one edge where a host lends both the bit and the tail, and declines "
           "it where either one is missing\n", lines.size());
}
