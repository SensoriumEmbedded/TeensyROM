# Building the C64 programs

`tools/build-c64.mjs` assembles the 6502 programs the firmware embeds (the menu, the
settings pages, help screens and bundled utilities) and writes each as a C header into
`Source/Teensy/TRMenuFiles/ROMs/`. Those headers are committed, and the firmware build
compiles whatever is there. The canonical reference for this step, including the
assembler versions and the project list, is [Source/C64/README.md](/Source/C64/README.md).

## Commands

```
npm run build:c64                                   # every project
npm run build:c64 -- --project MainMenuCRT          # one project (comma-separate several; case-insensitive)
npm run build:c64 -- --list                         # project names, and which assembler each needs
npm run build:c64 -- --verbose                      # the assemblers' full output
npm run build:c64 -- --project X --rom-dir <dir>    # write headers elsewhere, e.g. to compare with the committed ones
```

Every run first regenerates `Menu_Regs.i` from
`Source/Teensy/MinimalBoot/Common/Menu_Regs.h`, the register map the menu and the firmware
share, so a change to that header reaches both sides.

## When it is needed

After any change under `Source/C64/`, or to `Menu_Regs.h`, and before building the
firmware. That includes more than a project's own `.asm` files: several projects include
the same files (`c64defs.i`, `CommonDefs.i`, `Menu_Regs.i`), and some convert a `.prg` or
`.bin` straight into their header (the `BASIC` programs, `ASIDPlayer`'s character set). A
plain `npm run build:c64` rebuilds everything and is the safe choice; `--project` is for
when you know only that project uses the file. Commit the regenerated headers with the
source change:
CI reassembles every ACME project and fails if the result differs from what is committed
(`tools/build-c64.test.mjs`). The one project CI cannot check is `TRCustomBasicCommands`,
which needs Java for KickAssembler; rebuild it by hand when you touch it.

`VMHello` is the C64 half of the reference extension. It is packaged by
`tools/build-extension.mjs`, not embedded in the firmware, so it produces no header.

## Tools

Each tool can be pointed at with an environment variable (`ACME`, `KICKASS_JAR`,
`JAVA_HOME`); otherwise:

- **ACME**, for every project but one: `acme` on `PATH`, else a pinned, checksummed
  download into `tools/.cache/` (ignored by git) on Windows and macOS. On Linux install it
  with the package manager (`apt install acme`).
- **KickAssembler**, for `TRCustomBasicCommands` only: the pinned, checksummed download
  (it is not looked for on `PATH`). If the published file changes, the checksum fails;
  the error says how to point `KICKASS_JAR` at your own copy.
- **Java**, for KickAssembler: `java` on `PATH`. Never downloaded; install it yourself if
  you build that project.

A tool is only looked for when a project being built needs it, so ACME projects never ask
for Java. Pinned versions: [Source/C64/README.md](/Source/C64/README.md#prerequisites).

## When it fails

The build stops at the first failure and names the project and file. Each project builds
in its own `build/` folder, emptied first, which also holds the assembly report and the
VICE label file (`Labels`; `MainSymbols` and `CartSymbols` for MainMenuCRT).

## Adding a program

The list of projects and how each is built is `tools/c64-projects.json` (format at the top
of `tools/lib/c64-projects.mjs`). To put a new program in the menu, use the
`add-menu-program` skill.
