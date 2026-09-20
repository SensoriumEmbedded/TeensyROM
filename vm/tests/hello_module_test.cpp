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
    // Fixed sizes, so assert the rendered line and not just the numbers in it.
    assert(workspace.size() == 65536 && guest.size() == 524288);
    assert(lines[2] == "WORKSPACE 65536 GUEST 524288");
    // The file service really walked the package directory.
    assert(lines[3] == "PACKAGE FILES 3");

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

    printf("PASS: hello module loads on a base-profile host, rejects three bad hosts, "
           "publishes %zu ACK-gated lines, walks its package directory and repaints on input\n", lines.size());
}
