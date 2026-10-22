# Tests and CI

## Tests to run locally

| Command | Covers | Needs |
|---|---|---|
| `npm test` | The build tools (`tools/**/*.test.mjs`) and the firmware-source tests (`Source/Teensy/tests/**/*.test.js`). Includes rebuilding the C64 headers and comparing them with the committed ones; outside CI that test is skipped unless `acme` is on `PATH`. | Node |
| `npm run verify:extensions` | The extension loader, compiled and run on the host against packages the real packager writes. | A host C++ compiler (`g++`, or `--cxx` / `CXX`) |
| `python3 -m unittest discover -s tools/bench` | The bench scripts, against a fake board. | macOS or Linux (they use `termios` and `pty`) |

Run one test file with `node --test <path>`. Tests live next to the code they cover, as
`*.test.mjs` or `*.test.js`. None of them touch a board: the flasher's tests stop before it
looks for one.

## What CI runs

`.github/workflows/build.yml`, on every push to any branch, every pull request,
`Release_v*` tags and manual runs:

1. The three test suites above, in a job that installs ACME (not KickAssembler or Java,
   so `TRCustomBasicCommands` is not rebuilt there).
2. The TR and TR+ firmware, built in parallel with ccache. Release tags, and manual runs
   with "Disable ccache" ticked, compile from scratch so released files never come from a
   cache.
3. On a release tag, the two images published as the release.

CI also fails if `package.json` declares any dependencies: the build tools are
zero-dependency Node, and must stay that way.

`.github/workflows/experiment.yml` is a manual TR+ build for trying things out.

## Pinned versions

Build dependencies (the Teensy core, arduino-cli, the CRC32 library, the assemblers) are
pinned in the tools, and most also in the workflows and in the docs that state them.
`node tools/check-pins.mjs` lists every place and fails if they disagree, so a version bump
has to update all of them. It also looks up the latest releases, so it needs network
access. `.github/workflows/check-pins.yml` runs it every Monday and reports newer releases
in an issue. How a release is marked as rejected is in
[Build-System.md](/docs/Architecture/Build-System.md#dual-boot-linking-toolsbuild-firmwaremjs).

Keep pinned tool versions out of new docs, this skill included, unless you also add the
place to `tools/check-pins.mjs`.
