# NativeEthernet (vendored)

A copy of vjmuzik's [NativeEthernet](https://github.com/vjmuzik/NativeEthernet) library at upstream commit [`fe82aa7`](https://github.com/vjmuzik/NativeEthernet/commit/fe82aa7) (2021-08-09). That is the version Teensyduino 1.61.0 bundles (1.0.5), file for file. MinimalBoot and the main firmware both build against this copy instead of Teensyduino's. FNET, the TCP/IP stack under it, is still Teensyduino's.

## Why

The library's static-IP `begin()` waits for the Ethernet link with no time-out, so a static IP with no cable plugged in hangs whoever called it: the boot with TCP listen on, or the SwiftLink `ATC` command. Having the library in the tree lets that be fixed in one place for every caller in both images.

Upstream has one later commit, [`cdf6b3a`](https://github.com/vjmuzik/NativeEthernet/commit/cdf6b3a) (UDP writes honor `setSocketSize()`). TeensyROM never calls `setSocketSize()`, so it is not taken.

## How it's wired in

- The library lives here, under `MinimalBoot/Common/`, so both sketches can reach it. Arduino compiles only a sketch's root and its `src/` folder, so each sketch compiles it through one-line wrapper files, one per library `.cpp`:
  - main: `Source/Teensy/src/NativeEthernet/<File>.cpp`
  - MinimalBoot: `Source/Teensy/MinimalBoot/src/NativeEthernet/<File>.cpp`, empty when `VM_HOST_PROFILE` is defined. The extension host image is built from a copy of the MinimalBoot tree and links no Ethernet.
  - Each library `.cpp` stays its own translation unit. Don't merge the wrappers into one file: `Nativesocket.cpp` defines `yield()` away for the rest of its file.
- `Teensy.ino`, `MinimalBoot.ino` and `VMBoot.ino` include `NativeEthernet.h` (and `Teensy.ino` `NativeEthernetUdp.h`) by their path here.
- Don't write `#include <NativeEthernet.h>` (or any other of its headers in angle brackets) anywhere in the tree. arduino-cli would then also compile Teensyduino's copy, and the link fails with multiple-definition errors.
- MinimalBoot compiles the library whenever it isn't the extension host, including a build with `FeatTCPListen` turned off.
- To confirm this copy is the one in use:
  - **`npm run build:*`:** the build log has no `Using library NativeEthernet` line, and with `--keep-work` the build root's `libraries/` folders have no `NativeEthernet`.
  - **Arduino IDE:** with verbose compile output on, there should be no `Using library NativeEthernet` line.
- Only `src/` (without `examples/`), `LICENSE` and `AUTHORS` are here. Line endings are LF, as upstream.

## Updating it

The copy will track a tag in a fork, SensoriumEmbedded/NativeEthernet, branch `tr`, based on `fe82aa7`.

- To take an upstream change: apply it in the fork's `tr` branch, copy `src/` here (still no `examples/`), and update this README.
- Reapply the local modifications below whenever the contents are replaced.

**Local modifications:**

- `NativeEthernet.h`/`.cpp`: the static-IP `begin(mac, ip, dns, gateway, subnet)` takes a `timeout` in ms (default `ULONG_MAX`, which keeps the old wait) and returns `int`: 0 if the stack failed to start or the link was not up within the time-out, 1 once the link is up. The address is set before the wait, so a cable plugged in later comes up on it. TeensyROM passes the DHCP time-out setting.

## License

MIT (see `LICENSE`, Copyright Tino Hernandez), as are most files' headers (Copyright Paul Stoffregen). Two files keep the license of the Arduino Ethernet code they came from: `NativeDns.cpp`/`.h` are Apache 2.0, and `utility/NativeW5100.h` is GPL 2 or LGPL 2.1. Contributors are listed in `AUTHORS`. Teensyduino's copy carries the same terms, so vendoring it changes nothing about what the firmware links.
