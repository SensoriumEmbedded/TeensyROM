# Building the firmware

`tools/build-firmware.mjs` builds MinimalBoot and the main firmware and combines them into
the one `.hex` that releases ship and the flasher writes. Why the image has two parts, and
what the builder guards against, is in
[docs/Architecture/Build-System.md](/docs/Architecture/Build-System.md).

## Commands

```
npm run build:tr                      # TeensyROM (Fab 0.2/0.3)
npm run build:tr-plus                 # TeensyROM+ (Fab 0.4)
npm run build:tr-plus -- --force      # options go after --
node tools/build-firmware.mjs --target tr-plus --force # the same, without npm
```

The builder refuses an option it does not know, and a known option given twice, rather
than ignoring either (`tools/lib/cli-args.mjs` has the reasoning), so a misspelt one
(`--forse`) stops the build and names itself.

The output is `build/firmware/TeensyROM_<version>_full.hex` or
`build/firmware/TeensyROM+_<version>_full.hex`, where `<version>` is `TRVersion` in
`Source/Teensy/MinimalBoot/Common/Common_Defs.h`; a special build adds `-<tag>` and/or
`-dirty` to it (see `--tag` below). A build takes about three minutes per target. Before
it compiles anything it prints the build date it will stamp into the image, and the
version string when that differs from a release's.

## Before the first build

- **The Teensy core**, at the pinned version (see
  [Build-System.md](/docs/Architecture/Build-System.md#teensy-side)), installed through
  Teensyduino or the Arduino Boards Manager. The builder copies it into a private folder
  for each run and never changes the installed one. If the Arduino data folder is not in
  its default place, pass `--arduino-data <dir>`.
- **The CRC32 library**, in the `libraries` folder of your Arduino sketchbook. CI installs
  it with `arduino-cli lib install`. The builder looks for the sketchbook in
  `Documents/Arduino` (`~/Arduino` on Linux); if yours is elsewhere, for example because
  Windows moved Documents into OneDrive, pass `--arduino-user <dir>`.
- **arduino-cli**, from the `ARDUINO_CLI` environment variable (a path to the
  executable), else from `PATH`, else downloaded (pinned, checksummed) into `tools/.cache/`.

## Options

| Option | Effect |
|---|---|
| `--target tr` / `--target tr-plus` | Required. Which cartridge the image is for. |
| `--force` | Overwrite an existing output file. Without it the build stops before compiling. |
| `--yes` | For `--target tr` when `Fab04FeatureCtl.h` has `#define Fab04_Features` active: comment it out and carry on. This edits the tracked file, so it shows up in `git status`. |
| `--out <dir>` | Output folder (default `build/firmware`). On Windows keep it short: the build refuses a path that would push the toolchain past its length limit. |
| `--tag <name>` | A special build. The name goes on the version string in every image (`TeensyROM+ v0.9-pr65` on the C64 banner and in the serial version reply) and into the output names (`TeensyROM+_0.9-pr65_full.hex`, `..._VMBoot.TRH`). Letters, digits, `.` and `-`, up to what keeps the version inside the C64's 40-column banner row (24 characters for v0.9 on a TR+; a refusal says the limit). A tag long enough to crowd out a page's word (` Settings` and the like) makes that page show the version alone, which the build notes. |
| `--arduino-user <dir>`, `--arduino-data <dir>` | Where the sketchbook (libraries) and the Arduino data folder (Teensy core) are. |
| `--keep-work` | Keep the private build folder (`<out>/run-XXXX`, about 400 MB: the core copy, logs, symbol files) after a successful build. |
| `--skip-minimal-build`, `--skip-teensy-build`, `--skip-combine` | Build only part of the image. With `--skip-combine` the private build folder is always kept, since that is where the parts are. |
| `--ccache` | macOS and Linux only, with `ccache` on `PATH`: reuse earlier compiles. |
| `--no-extensions`, `--skip-extension-build`, `--host-sketch <dir>`, `--host-code-kib <64 or 96>` | The extension loader and host, `--target tr-plus` only: build the TR+ without the loader (it gets the `_noext` name), skip building the stock host's `.TRH`, build a different host and nothing else, or raise that host's code ceiling. `--with-extensions` is refused: extensions are on by default. [Build-System.md](/docs/Architecture/Build-System.md) has the detail. |

A tree with uncommitted edits to the image sources (`Source/`, the linker files; the
`Fab04FeatureCtl.h` toggle is left out of the check) needs no flag: its version string
ends in `*`, its output names in `-dirty`, and its build date is the build's own time
rather than the commit's. The build lists the files that made it dirty. The double-click
`_Build-TeensyROM*.ps1` wrappers pass no options, so `--tag` needs the command line.

## Recipes

- **Both cartridges:** `npm run build:tr -- --force && npm run build:tr-plus -- --force`
- **A review or test build, told apart on the board:**
  `npm run build:tr-plus -- --tag pr65 --out C:/tr-build/pr65` shows `TeensyROM+ v0.9-pr65`
  on the C64 banner and in `npm run flash -- --check`, with `*` after it while the tree has
  uncommitted edits.
- **Memory report only** (about a minute, to check a change against the RAM budgets in
  [Constraints.md](/docs/Architecture/Constraints.md)):
  `npm run build:tr-plus -- --skip-minimal-build --skip-combine`. It leaves its
  `run-XXXX` folder behind; delete it afterwards.
- **Rebuild what a board runs:** check out the commit it was built from and build. The same
  commit gives a byte-identical hex, because the build date comes from the commit. To
  find the commit from the date a board reports (commit time, in UTC):
  `TZ=UTC git log --date=format-local:'%b %e %Y, %H:%M:%S' --format='%h %cd %s'`
  A board whose version ends in `*` runs a build of a dirty tree: its date is that build's
  own time and names no commit.

## When it fails

| Message | What to do |
|---|---|
| `... already exists. Re-run with --force to overwrite.` | Pass `--force`. |
| `Fab04_Features is #define'd ... but --target tr was requested` | Comment out the define, build `--target tr-plus` instead, or pass `--yes`. |
| `Teensy core ... not found at ...` | Install the pinned core, or pass `--arduino-data`. |
| `Use a shorter --out path` | Windows path limit: pick a short folder such as `C:/tr-build`. |
| `--tag "..." may only use letters, digits ...` or `... is the most that fits ...` | The tag has a character the C64 cannot show (`_` among them), or it is longer than the 40-column banner row allows; pick a shorter one. |
| A library not found during compile | The sketchbook is not where the builder looked: `--arduino-user`. |
| The build fails after linking because an mbedTLS symbol came back | TLS is compiled out on purpose; see Build-System.md. |
| `[main] links __ssvfscanf_r (...)` (or `__pow_log_data`, `_strtod_l`, ...) | New code calls `sscanf`, `pow`, `atof` or a similar newlib routine that costs RAM1. Parse or compute the value directly; the list and why is in `tools/lib/heavy-libc.mjs`, see Build-System.md. |

A failed build keeps its `run-XXXX` folder in the output folder; the logs in it explain
the failure. Delete these folders once you are done, since each holds a few hundred MB.

After building both cartridges, the newest image is the TeensyROM+ one, so a plain
`npm run flash` with only a TeensyROM attached refuses it. Use `npm run flash -- --all`,
which picks the image for each board's type, or name the image with `--hex`.

Do not build with the Teensyduino release that Build-System.md marks as known-bad, and do
not change source code to work around it: the fault is in that release's compiler.
