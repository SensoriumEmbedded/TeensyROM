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

    // Up on a host that lends no exit does nothing at all -- not a fault, not a
    // repaint. That is the fallback half of "capability, then fallback", and it
    // is the state this host is in: make() publishes neither the bit nor the tail.
    assert(host.bytes == sizeof(VmHost) && !(host.services & VM_SERVICE_EXIT));
    VmInput up{ 2, 0, 0, 0x81 };
    loaded->input(&up);
    assert(!loaded->packet(&packet));
    assert(!VmNativeHost::lastFailure);
    VmInput released{ 0, 0, 0, 0x81 };
    loaded->input(&released);

    // Input recolours and forces a repaint from row zero.
    VmInput press{ 1, 0, 0, 0x81 };
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

    // On a host that does lend one, up takes it. Reloading the module is what
    // resets its static edge state, and is also how the firmware gets here: the
    // host is built once, before vm_entry.
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
        // the rest of the module's input handling.
        withExit->input(&released);
        withExit->input(&press);
        assert(withExit->packet(&packet) && packet.payload[0] == 0);
        assert(VmNativeHost::exitCalls == 1);
    }

    printf("PASS: hello module loads on a base-profile host, rejects three bad hosts, "
           "publishes %zu ACK-gated lines, walks its package directory, repaints on input, and takes "
           "the exit service on one edge where a host lends it and ignores it where none does\n", lines.size());
}
