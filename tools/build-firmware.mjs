// SPDX-License-Identifier: MIT
//
// Node port of Source/Teensy/tools/Build-DualBoot.ps1. Builds the dual-boot TeensyROM
// image (MinimalBoot at 0x60000000 + the main image at 0x60060000, combined into one hex;
// for --target tr-plus the same hex also carries the extension host image at 0x60280000)
// for one of two release targets:
//
//   --target tr        plain TeensyROM (Fab 0.2/0.3). No extension loader: it needs the
//                       full DMA only Fab 0.4 has.
//   --target tr-plus   TeensyROM+ (Fab 0.4), with the extension loader. Always includes MPE
//                       once MPE is on main; until then this builds today's TR+ image under
//                       today's name.
//
// Unlike Build-DualBoot.ps1, this never touches the installed Teensy core: the boot/linker
// file swap happens in a private copy made fresh for this run, so nothing about your
// toolchain changes because you ran a build. See the "installed core unchanged" check below.
//
// The original script's two Read-Host prompts become flags, so a non-interactive run fails
// fast instead of hanging:
//   --yes    confirm disabling an active Fab04_Features #define when building --target tr
//   --force  overwrite an existing output file
//   --keep-work  keep the private build root (a full copy of the Teensy core, plus logs and
//            symbol files, ~400 MB) after a successful build. By default it is removed; a
//            failed build always keeps it, since its logs are what explain the failure, and so
//            does --skip-combine, whose per-image results exist only there.
//   --no-extensions  build a TR+ without the extension loader: the two images it carried
//            before the loader existed, from the stock linker scripts. It lands under
//            TeensyROM+_<ver>_noext_full.hex, not the shipping name. Refused on --target
//            tr, which never carries the loader.
//   --host-sketch <dir>  put a different program in the extension slot. The directory
//            holds exactly one .ino plus whatever else it needs, files only -- no
//            subdirectories; those files are overlaid onto the MinimalBoot sketch, which
//            is how the stock host is built too. This is the supported seam for a
//            third-party extension host -- see docs/Architecture/Extension-Hosts.md.
//            It lands under TeensyROM+_<ver>_<dir>_full.hex, not the shipping name: the
//            slot holds a program this repo did not write.
//
// --ccache routes compiles through ccache (which must be on PATH; not supported on Windows).
// Two things that only matter with it on: the build root is a fixed run-ccache-<target>
// directory, cleared each run, because the compiler's working directory and -I paths are
// part of ccache's key (-g is on) and a fresh mkdtemp name would miss every time; and
// pinning SOURCE_DATE_EPOCH to the commit (below) is what lets the __DATE__/__TIME__ files
// hit at all, until the next commit.
//
// The final combine step uses lib/legacy-hex-combine.mjs, a literal port of
// HexCombineUtil/HexCombine.exe's algorithm (its source was recovered 2026-09-13). That
// port, and this build overall, has been verified end to end: flashed to a real Teensy in a
// C128 and confirmed booting to the TeensyROM menu, for both --target tr and --target
// tr-plus. This is ahead of the original plan, which kept HexCombine.exe itself (and a
// Windows runner) for Phase 1 and left the port for Phase 4 pending a byte-for-byte
// comparison; the .exe's actual source made that comparison possible now instead. The ACME
// / C64 side is still ported in a later phase.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { resolveArduinoCli, defaultArduinoDataDir, defaultArduinoUserDir } from './lib/toolchain.mjs';
import { checkFlashHeadroom, formatFlashHeadroom } from './lib/flash-headroom.mjs';
import { legacyCombineHex } from './lib/legacy-hex-combine.mjs';
import { combineHex, FLASH_BASE, MAIN_BASE, VM_BASE, VM_LIMIT } from './lib/hex.mjs';
import {
  minimalLinkerScript, mainLinkerScript, extensionLinkerScript, extensionBootdata, VM_EXTENSIONS_DEFINE,
  patchStartupForUsbDisabled, patchYieldForUsbDisabled, flashBudget,
} from './lib/extension-image.mjs';

const TEENSY_CORE_VERSION = '1.61.0';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const args = process.argv.slice(2);

function flag(name) {
  return args.includes(name);
}

// Extensions are on by default for --target tr-plus, so an argument this script does not
// recognise cannot be ignored: a misspelled opt-out (--no-extension, --noextensions,
// --no_extensions, -no-extensions) would otherwise ship the loader in an image the caller
// asked to build without it. Anything not in these two sets is a refusal.
const KNOWN_OPTIONS = new Set(['--target', '--out', '--arduino-data', '--arduino-user', '--host-sketch']);
const KNOWN_FLAGS = new Set([
  '--yes', '--force', '--keep-work', '--skip-teensy-build', '--skip-minimal-build',
  '--skip-combine', '--skip-extension-build', '--no-extensions', '--with-extensions', '--ccache',
]);
// One pass, so an option's value is never mistaken for an option and a repeat cannot be
// quietly dropped. Reading each option with its own indexOf() took the *first* occurrence
// and ignored the rest, which is the wrong end for the one shape that produces a repeat in
// practice: `npm run <script> -- --opt <value>` appends the caller's argument after the
// script's own, so the override lost and the script's value won without a word. For
// --host-sketch that means `npm run build:example-host -- --host-sketch Source/Teensy/MyHost`
// builds Source/Teensy/ExampleHost and exits 0 -- the stock host under the caller's name,
// which is the exact failure the refusals below exist to prevent. Refusing is the noisy
// direction and costs a caller who meant to override nothing but naming the script directly.
const optionValues = new Map();
for (let i = 0; i < args.length; i++) {
  if (KNOWN_OPTIONS.has(args[i])) {
    const name = args[i], value = args[i + 1];
    if (!value || value.startsWith('--')) throw new Error(`Missing value for ${name}`);
    if (optionValues.has(name)) {
      throw new Error(`${name} given more than once (${optionValues.get(name)}, then ${value}). ` +
        'Only one can take effect and the other would be ignored silently, so neither is. ' +
        `If this came from \`npm run <script> -- ${name} ${value}\`, the script already passes ` +
        `${name}; run tools/build-firmware.mjs directly instead.`);
    }
    optionValues.set(name, value);
    i++;
    continue;
  }
  if (!KNOWN_FLAGS.has(args[i])) {
    throw new Error(`Unknown argument ${args[i]}. Known arguments: ` +
      [...KNOWN_OPTIONS, ...KNOWN_FLAGS].sort().join(' '));
  }
}
// Every caller below runs after the scan above, so the map is populated.
function option(name, fallback) {
  return optionValues.has(name) ? optionValues.get(name) : fallback;
}

const target = option('--target', null);
if (!['tr', 'tr-plus'].includes(target)) {
  throw new Error('Use --target tr or --target tr-plus');
}
const fab04Features = target === 'tr-plus';
const yes = flag('--yes');
const force = flag('--force');
const keepWork = flag('--keep-work');
const skipTeensyBuild = flag('--skip-teensy-build');
const skipMinimalBuild = flag('--skip-minimal-build');
const skipCombine = flag('--skip-combine');
// Extensions ride with the Fab 0.4 feature set rather than a flag of their own. The
// install path blanks the screen through the full DMA Fab04_FullDMACapable gates, so a
// fab 0.2/0.3 extensions build would leave the VIC painting a frozen menu for the whole
// erase. --no-extensions builds exactly the two images the TR+ carried before, from the
// stock linker scripts, and nothing below runs.
// Both branches open with "no longer exists" on purpose: the flag is refused on every
// target now, so a --target tr message that only said "needs --target tr-plus" would be
// routing the caller to a second refusal.
if (flag('--with-extensions')) {
  throw new Error(fab04Features
    ? '--with-extensions no longer exists: --target tr-plus builds the extension loader by ' +
      'default. Drop the flag, or pass --no-extensions to build a TR+ without the loader.'
    : '--with-extensions no longer exists, and a plain TR could not carry the loader anyway: ' +
      'it requires the Fab 0.4 full DMA a plain TR does not have. Drop the flag; ' +
      '--target tr-plus builds the loader by default.');
}
// Refused rather than ignored on a plain TR, for the same reason --host-sketch is below:
// a flag that silently does nothing tells the caller it opted out of something that was
// never there, and the next such flag they reach for may not be harmless.
if (!fab04Features && flag('--no-extensions')) {
  throw new Error('--no-extensions needs --target tr-plus: a plain TR never carries the ' +
    'extension loader, so there is nothing to opt out of');
}
const withExtensions = fab04Features && !flag('--no-extensions');
const skipExtensionBuild = flag('--skip-extension-build');

// Which program goes in the extension slot. The default is this repo's own host;
// a third-party host is the same build with its sketch directory swapped in, which
// is the whole of the mechanism (docs/Architecture/Extension-Hosts.md). Refused
// where the extension image is not built at all, rather than ignored: a flag that
// silently does nothing here ships the stock host under the caller's own name.
const hostSketchOption = option('--host-sketch', null);
if (hostSketchOption !== null && !withExtensions) {
  throw new Error(fab04Features
    ? '--host-sketch has nothing to build with --no-extensions'
    : '--host-sketch needs --target tr-plus: a plain TR reserves no extension slot');
}
if (hostSketchOption !== null && skipExtensionBuild) {
  throw new Error('--host-sketch and --skip-extension-build contradict each other');
}
const hostSketch = path.resolve(hostSketchOption ?? path.join(root, 'Source/Teensy/VMBoot'));
// The overlay is read here rather than where it is used, because the extension image is
// built last: a host sketch that is a file, or holds no .ino or two, was otherwise refused
// only after the minimal and main images had compiled, and a path that was not a directory
// arrived as a raw ENOTDIR stack rather than as a refusal. Everything wrong with
// --host-sketch now costs a process spawn, which is what the "not found" case already cost.
let hostOverlay = [], hostEntryIno = null;
if (withExtensions && !skipExtensionBuild) {
  if (!fs.existsSync(hostSketch)) {
    throw new Error(`Host sketch directory not found: ${hostSketch}`);
  }
  // Checked by name, because readdirSync on a file answers with an ENOTDIR stack trace --
  // a crash report where this file's every other bad argument gets a sentence.
  if (!fs.statSync(hostSketch).isDirectory()) {
    throw new Error(`Host sketch is not a directory: ${hostSketch}`);
  }
  // Classified by stat rather than by dirent, so a symlink to a file still reads as the
  // file it points at -- copyFileSync follows it too. A symlink to a directory, and one
  // pointing at nothing, both land in notFiles and are refused with everything else.
  const entries = fs.readdirSync(hostSketch).map((name) => {
    let stats = null;
    try { stats = fs.statSync(path.join(hostSketch, name)); } catch { /* dangling symlink */ }
    return { name, file: stats !== null && stats.isFile() };
  });
  // Refused rather than skipped. The overlay copies files, so a host keeping sources in a
  // subdirectory -- src/, which is an ordinary Arduino sketch layout -- would otherwise
  // build without them: either a link error blaming something else, or, for a
  // subdirectory shadowing one of MinimalBoot's own, a clean build of the wrong program
  // shipped under the caller's name. Silent is the one thing it must not be.
  const notFiles = entries.filter((e) => !e.file).map((e) => e.name);
  if (notFiles.length) {
    throw new Error(`${hostSketch} may hold only files: the overlay does not descend into ` +
      `${notFiles.join(', ')}. Move those sources up beside the .ino.`);
  }
  hostOverlay = entries.map((e) => e.name);
  const inos = hostOverlay.filter((f) => f.endsWith('.ino'));
  if (inos.length !== 1) {
    throw new Error(`${hostSketch} must hold exactly one .ino (the sketch entry point), found ${inos.length}`);
  }
  hostEntryIno = inos[0];
}

const useCcache = flag('--ccache');
if (useCcache && process.platform === 'win32') {
  throw new Error('--ccache is not supported on Windows');
}
const outDir = path.resolve(option('--out', path.join(root, 'build', 'firmware')));

const sdk = path.resolve(option('--arduino-data', defaultArduinoDataDir()));
const arduinoUser = path.resolve(option('--arduino-user', defaultArduinoUserDir()));

const read = (p) => fs.readFileSync(p, 'utf8');
const write = (p, contents) => {
  fs.mkdirSync(path.dirname(p), { recursive: true });
  fs.writeFileSync(p, contents);
};
const sha256 = (buf) => crypto.createHash('sha256').update(buf).digest('hex');

function run(exe, argv, env) {
  const result = spawnSync(exe, argv, { cwd: root, env, encoding: 'utf8', windowsHide: true, maxBuffer: 48 * 1024 * 1024 });
  if (result.error || result.status) {
    throw new Error(`${exe} ${argv.join(' ')} failed\n${result.error ?? ''}\n${(result.stdout ?? '') + (result.stderr ?? '')}`.slice(-16000));
  }
  return (result.stdout ?? '') + (result.stderr ?? '');
}

// --- Build date: GCC takes __DATE__/__TIME__ from SOURCE_DATE_EPOCH (formatted in UTC),
// so the firmware's build date is the HEAD commit's time. Rebuilding a commit reproduces
// its hex, and the date still says which commit it came from. Uncommitted edits don't
// move it. A SOURCE_DATE_EPOCH already in the environment wins, per the
// reproducible-builds convention; outside a git checkout it falls back to now. ---
function sourceDateEpoch() {
  if (process.env.SOURCE_DATE_EPOCH) return process.env.SOURCE_DATE_EPOCH;
  const git = spawnSync('git', ['log', '-1', '--format=%ct'], { cwd: root, encoding: 'utf8', windowsHide: true });
  const commitTime = git.status === 0 ? git.stdout.trim() : '';
  if (/^\d+$/.test(commitTime)) return commitTime;
  console.warn('WARNING: no git commit found; using the current time as the build date');
  return String(Math.floor(Date.now() / 1000));
}
const SOURCE_DATE_EPOCH = sourceDateEpoch();
console.log(`Build date (SOURCE_DATE_EPOCH): ${new Date(SOURCE_DATE_EPOCH * 1000).toISOString()}`);

// --- TRVersion, matching Get-TRVersion in Build-DualBoot.ps1 ---
const commonDefsPath = path.join(root, 'Source/Teensy/MinimalBoot/Common/Common_Defs.h');
const trVersionMatch = read(commonDefsPath).match(/^\s*#define\s+TRVersion\s+"([^"]+)"/m);
if (!trVersionMatch) throw new Error(`TRVersion not found in ${commonDefsPath}`);
const trVersion = trVersionMatch[1];
console.log(`TRVersion: ${trVersion}`);

// --- Fab04 guard, matching Test-Fab04FeaturesDefined / Disable-Fab04FeaturesDefine ---
// Fab04FeatureCtl.h has a commented-out "#define Fab04_Features" that overrides the target
// flag if uncommented. If it's active but --target tr was requested, the build would
// silently produce a TR+ image while claiming to be plain TR.
const fab04CtlPath = path.join(root, 'Source/Teensy/MinimalBoot/Common/Fab04FeatureCtl.h');
// `#\s*define` because C allows whitespace between the # and the directive: `# define
// Fab04_Features` compiles exactly like `#define Fab04_Features`, so a pattern that missed
// it would leave this guard blind to an active define and ship a Fab 0.4 image under the
// plain-TR name -- the one outcome the guard exists to prevent.
const FAB04_DEFINE = /^\s*#\s*define\s+Fab04_Features\b/m;
const fab04Active = FAB04_DEFINE.test(read(fab04CtlPath));
if (!fab04Features && fab04Active) {
  if (!yes) {
    throw new Error(
      `Fab04_Features is #define'd in ${fab04CtlPath}, but --target tr was requested.\n` +
      'This mismatch would build a TR+ image while labeled as plain TR.\n' +
      'Comment out the #define yourself, re-run with --target tr-plus, or pass --yes to have this comment it out and continue.',
    );
  }
  // /gm, not /m: a second active #define would otherwise survive this "repair" and the build
  // would ship Fab 0.4 features under the plain-TR name. The replacement keeps the line's
  // own spelling rather than normalising it, and is deliberately narrower than FAB04_DEFINE
  // ([ \t] where that has \s) so that re-testing with FAB04_DEFINE stops the build on any
  // shape this pattern misses instead of passing for repaired. That test runs against the
  // proposed text, before the write: an incomplete repair must not reach the tracked file,
  // or the run that refused to continue still leaves an edit behind for someone to commit.
  const updated = read(fab04CtlPath).replace(/^([ \t]*)(#[ \t]*define[ \t]+Fab04_Features\b.*)$/gm, '$1// $2');
  if (FAB04_DEFINE.test(updated)) {
    throw new Error(`Fab04_Features would still be #define'd in ${fab04CtlPath} after commenting it out, ` +
      'so it has been left unchanged; comment it out by hand.');
  }
  fs.writeFileSync(fab04CtlPath, updated);
  console.log(`Fab04_Features #define commented out in ${fab04CtlPath}`);
}

// --- Output path ---
// A --no-extensions TR+ is a materially different image from the shipping one. Its own
// name keeps it from overwriting, or being published as, the build that carries the loader.
// A --host-sketch TR+ is materially different in the same way and for a sharper reason: the
// slot carries a program this repo did not write, and under the shipping name the two are
// one `ls` apart. It takes the sketch directory's name, so `npm run build:example-host`
// cannot leave an LED blinker sitting where the release hex goes, and so the --hex a host
// author hands to build-host-package.mjs names the host they meant.
const outputStem = target === 'tr-plus' ? `TeensyROM+_${trVersion}` : `TeensyROM_${trVersion}`;
const hostMark = hostSketchOption === null ? '' : `_${path.basename(hostSketch)}`;
const finalOutput = path.join(outDir,
  `${outputStem}${fab04Features && !withExtensions ? '_noext' : ''}${hostMark}_full.hex`);
if (!skipCombine && fs.existsSync(finalOutput) && !force) {
  throw new Error(`${finalOutput} already exists. Re-run with --force to overwrite.`);
}

// --- Locate the pinned Teensy core; never write to it ---
const hardwareCore = path.join(sdk, 'packages/teensy/hardware/avr', TEENSY_CORE_VERSION);
if (!fs.existsSync(hardwareCore)) {
  throw new Error(`Teensy core ${TEENSY_CORE_VERSION} not found at ${hardwareCore}. Install it via Teensyduino/Boards Manager, or pass --arduino-data.`);
}
const installedCoreDir = path.join(hardwareCore, 'cores/teensy4');
const guardedFiles = ['bootdata.c', 'imxrt1062_t41.ld'].map((f) => path.join(installedCoreDir, f));
const hashGuardedFiles = () => guardedFiles.map((f) => sha256(fs.readFileSync(f))).join(',');
const installedCoreHashBefore = hashGuardedFiles();

fs.mkdirSync(outDir, { recursive: true });
let runRoot;
if (useCcache) {
  runRoot = path.join(outDir, `run-ccache-${target}`);
  fs.rmSync(runRoot, { recursive: true, force: true });
  fs.mkdirSync(runRoot);
} else {
  runRoot = fs.mkdtempSync(path.join(outDir, 'run-'));
}
if (process.platform === 'win32' && runRoot.length > 70) {
  throw new Error('Use a shorter --out path (e.g. C:/tr-build) to stay within Windows toolchain path limits');
}
console.log(`Private build root: ${runRoot}`);

// --- Private copy of the Teensy core + Arduino data dir. The linker-file swap happens
// only here, so the installed core is never modified (fixes the stale bootdata.c bug). ---
const privateData = path.join(runRoot, 'Arduino15');
fs.cpSync(hardwareCore, path.join(privateData, 'packages/teensy/hardware/avr', TEENSY_CORE_VERSION), { recursive: true });
fs.symlinkSync(path.join(sdk, 'packages/teensy/tools'), path.join(privateData, 'packages/teensy/tools'), process.platform === 'win32' ? 'junction' : 'dir');
for (const entry of fs.readdirSync(sdk, { withFileTypes: true })) {
  if (entry.isFile()) fs.copyFileSync(path.join(sdk, entry.name), path.join(privateData, entry.name));
}
const privateCore = path.join(privateData, 'packages/teensy/hardware/avr', TEENSY_CORE_VERSION, 'cores/teensy4');
const linkers = path.join(root, 'tools/BootLinkerFiles');

const cli = resolveArduinoCli(path.join(root, 'tools/.cache'));
const env = {
  ...process.env,
  ARDUINO_DIRECTORIES_DATA: privateData,
  ARDUINO_DIRECTORIES_USER: arduinoUser,
  SOURCE_DATE_EPOCH,
  // The CI toolchain is reinstalled every run, so its mtime says nothing about the compiler.
  ...(useCcache && { CCACHE_COMPILERCHECK: 'content' }),
};

const armBin = path.join(sdk, 'packages/teensy/tools/teensy-compile/11.3.1/arm/bin/arm-none-eabi-');
const exeSuffix = process.platform === 'win32' ? '.exe' : '';

// --- ccache shim: the platform recipes quote "{compiler.path}{build.toolchain}<tool>", so
// there's no room for a "ccache " prefix. Instead, point compiler.path at a directory whose
// arm/bin/ (build.toolchain) wraps gcc/g++ in ccache and symlinks every other tool. ---
const compilerPathProps = [];
if (useCcache) {
  run('ccache', ['--version'], env);
  const realBin = path.dirname(armBin);
  const shimRoot = path.join(runRoot, 'ccache-shim');
  const shimBin = path.join(shimRoot, 'arm/bin');
  fs.mkdirSync(shimBin, { recursive: true });
  for (const tool of fs.readdirSync(realBin)) {
    const shim = path.join(shimBin, tool);
    if (tool === 'arm-none-eabi-gcc' || tool === 'arm-none-eabi-g++') {
      fs.writeFileSync(shim, `#!/bin/sh\nexec ccache "${path.join(realBin, tool)}" "$@"\n`, { mode: 0o755 });
    } else {
      fs.symlinkSync(path.join(realBin, tool), shim);
    }
  }
  compilerPathProps.push('--build-property', `compiler.path=${shimRoot}/`);
  console.log(`ccache enabled via ${shimBin}`);
}

// FNET's own default is FNET_CFG_TLS=2 (fnet_user_config.h in the installed Teensy core,
// behind an #ifndef), which links mbedTLS into MinimalBoot and the main image. build()
// appends this to every image it compiles, the extension host included.
const TLS_OFF = '-DFNET_CFG_TLS=0';

// `suffix` picks a stock BootLinkerFiles pair; `ld`/`bootdata` override it with
// generated text, which is how the extension image gets its own flash slot.
function writeLinkerFiles(suffix, { ld, bootdata } = {}) {
  fs.writeFileSync(path.join(privateCore, 'bootdata.c'),
    bootdata ?? read(path.join(linkers, `bootdata.c.${suffix}`)));
  fs.writeFileSync(path.join(privateCore, 'imxrt1062_t41.ld'),
    ld ?? read(path.join(linkers, `imxrt1062_t41.ld.${suffix}`)));
}

function build(name, { inoPath, fqbn, suffix, elfStem, ld, bootdata, usbType, extraDefs = '' }) {
  console.log(`\n[${name}] Building`);
  writeLinkerFiles(suffix, { ld, bootdata });
  const buildDir = path.join(runRoot, name);
  const props = run(cli, ['compile', '--fqbn', fqbn, '--build-path', buildDir, ...compilerPathProps, '--show-properties', inoPath], env);
  const defsMatch = props.match(/^build\.flags\.defs=(.*)$/m);
  if (!defsMatch) throw new Error('Could not retrieve build.flags.defs from --show-properties');
  const defs = defsMatch[1].trim() + (fab04Features ? ' -DFab04_Features' : '') + extraDefs + ' ' + TLS_OFF;

  const compileArgs = ['compile', '--fqbn', fqbn, '--build-path', buildDir, ...compilerPathProps,
    '--build-property', `build.flags.defs=${defs}`];
  // The extension image needs USB fully off, which is not one of the core's
  // `usb=` FQBN menu choices. The FQBN still says usb=serial; this wins.
  if (usbType) compileArgs.push('--build-property', `build.usbtype=${usbType}`);
  compileArgs.push(inoPath);

  const log = run(cli, compileArgs, env);
  write(path.join(runRoot, `${name}.log`), log);
  console.log(log.split(/\r?\n/).filter((l) => /Memory Usage|RAM1:|RAM2:|FLASH:/.test(l)).join('\n'));

  const elf = path.join(buildDir, `${elfStem}.ino.elf`);
  const hex = path.join(buildDir, `${elfStem}.ino.hex`);
  if (!fs.existsSync(hex)) throw new Error(`Hex file not found: ${hex}`);
  const sizeKB = (fs.statSync(hex).size / 1024).toFixed(2);
  console.log(`  Hex: ${hex} (${sizeKB} KB)`);

  // Symbol/section-size capture: the Phase 3 linker-safety checks (ITCM budget,
  // heap/stack overlap) read these files, so it has to happen for every image.
  const symbols = run(armBin + 'nm' + exeSuffix, ['-n', '-C', elf], env);
  write(path.join(runRoot, `${name}.nm`), symbols);
  if (!/^[0-9a-fA-F]+ \S /m.test(symbols)) throw new Error(`nm listed no symbols for ${name}; the linked-symbol checks cannot run`);
  if (/mbedtls/i.test(symbols)) throw new Error(`mbedTLS is linked into ${name}; ${TLS_OFF} should have kept it out`);
  const sizes = run(armBin + 'size' + exeSuffix, ['-A', elf], env);
  write(path.join(runRoot, `${name}.size`), sizes);

  return { name, elf, hex };
}

let minimalImage = null, teensyImage = null, extensionImage = null;

if (withExtensions) {
  const budget = flashBudget();
  console.log(`\nReserving ${budget.extensionKB}K at 0x${VM_BASE.toString(16)} for the extension image.` +
    ` minimal ${budget.stockMinimalKB}K -> ${budget.minimalKB}K, main ${budget.stockMainKB}K -> ${budget.mainKB}K.`);
}

if (!skipMinimalBuild) {
  minimalImage = build('minimal', {
    inoPath: path.join(root, 'Source/Teensy/MinimalBoot/MinimalBoot.ino'),
    fqbn: 'teensy:avr:teensy41:usb=serial,speed=600,opt=o2std,keys=en-us',
    suffix: 'orig',
    elfStem: 'MinimalBoot',
    ld: withExtensions ? minimalLinkerScript(linkers) : undefined,
    extraDefs: withExtensions ? VM_EXTENSIONS_DEFINE : '',
  });
}

if (!skipTeensyBuild) {
  teensyImage = build('main', {
    inoPath: path.join(root, 'Source/Teensy/Teensy.ino'),
    fqbn: 'teensy:avr:teensy41:usb=serialmidi,speed=600,opt=o2std,keys=en-us',
    suffix: 'upper',
    elfStem: 'Teensy',
    ld: withExtensions ? mainLinkerScript(linkers) : undefined,
    extraDefs: withExtensions ? VM_EXTENSIONS_DEFINE : '',
  });
}

if (withExtensions && !skipExtensionBuild) {
  // Arduino compiles a sketch directory as a unit, so the extension image is
  // assembled from the minimal sketch with its own top-level .ino and build
  // profile swapped in. A plain file copy: nothing is generated or rewritten,
  // and every replacement is an ordinary committed file.
  //
  // --host-sketch points that overlay at a different directory, which is how a
  // third-party host is built: same memory map, same slot, different program.
  // See docs/Architecture/Extension-Hosts.md. Every file in the directory is
  // overlaid, not a fixed list, so a host that wants a third file does not have
  // to come back here and edit the build. hostOverlay and hostEntryIno were read
  // and checked up with the other argument handling, before anything compiled.
  // Arduino requires the sketch directory and its entry .ino to share a name, so the
  // directory name is the caller's to choose -- and runRoot is already occupied. build()
  // puts each image's build tree at runRoot/<name> (minimal, main, extension), the private
  // SDK copy is runRoot/Arduino15 and the ccache shim runRoot/ccache-shim. A host whose
  // entry point is main.ino would have been staged straight onto the main image's finished
  // build tree: cpSync merges, so nothing errors, and the extension image then compiles
  // against that mixture. One directory of its own, so the caller's name can be anything.
  const elfStem = path.basename(hostEntryIno, '.ino');
  const sketch = path.join(runRoot, 'host', elfStem);
  fs.cpSync(path.join(root, 'Source/Teensy/MinimalBoot'), sketch, { recursive: true });
  fs.rmSync(path.join(sketch, 'MinimalBoot.ino'));
  for (const file of hostOverlay) {
    fs.copyFileSync(path.join(hostSketch, file), path.join(sketch, file));
  }
  // Only this image builds with USB compiled out, so only its private core copy
  // needs the two unguarded-USB fixes. Never applied to the installed core.
  for (const [file, patch] of [['yield.cpp', patchYieldForUsbDisabled], ['startup.c', patchStartupForUsbDisabled]]) {
    const target = path.join(privateCore, file);
    write(target, patch(read(target)));
  }

  extensionImage = build('extension', {
    inoPath: path.join(sketch, hostEntryIno),
    fqbn: 'teensy:avr:teensy41:usb=serial,speed=600,opt=o2std,keys=en-us',
    elfStem,
    ld: extensionLinkerScript(linkers),
    bootdata: extensionBootdata(linkers),
    usbType: 'USB_DISABLED',
    extraDefs: ' -DVM_HOST_PROFILE',
  });
} else if (withExtensions) {
  console.log('\n[extension] Skipped (--skip-extension-build)');
}

// The private copy is the only thing the linker-file swap touched. Confirm the fix holds.
const installedCoreHashAfter = hashGuardedFiles();
if (installedCoreHashBefore !== installedCoreHashAfter) {
  throw new Error(`Installed Teensy core changed during the build (${guardedFiles.join(', ')}). This should be impossible with the private-copy build.`);
}
console.log('\nInstalled Teensy core unchanged (bootdata.c, imxrt1062_t41.ld hash match before/after).');

if (!skipCombine) {
  if (!minimalImage || !teensyImage) throw new Error('Combining requires both images; pass --skip-combine yourself if that is intentional');
  if (withExtensions && !extensionImage) throw new Error('Combining an extension build requires the extension image; pass --skip-combine yourself if that is intentional');
  console.log('\n[combine] Combining hex images');
  let combined;
  if (withExtensions) {
    // combineHex refuses an overlap or an out-of-bounds byte outright, so a
    // mis-sized image is caught here rather than by corrupting its neighbour.
    const merged = combineHex([
      { name: 'minimal', text: read(minimalImage.hex), start: FLASH_BASE, end: MAIN_BASE },
      { name: 'main', text: read(teensyImage.hex), start: MAIN_BASE, end: VM_BASE },
      { name: 'extension', text: read(extensionImage.hex), start: VM_BASE, end: VM_LIMIT },
    ]);
    for (const region of merged.regions) {
      const used = region.usedEnd - region.start, capacity = region.end - region.start;
      console.log(`  ${region.name.padEnd(9)} ${(used / 1024).toFixed(1)}K of ${(capacity / 1024).toFixed(0)}K` +
        ` (${(100 * used / capacity).toFixed(1)}%)`);
    }
    combined = merged.hex;
  } else {
    combined = legacyCombineHex(read(minimalImage.hex), read(teensyImage.hex));
  }
  write(finalOutput, combined);
  if (!fs.existsSync(finalOutput)) throw new Error(`Combined hex not created: ${finalOutput}`);
  const finalKB = (fs.statSync(finalOutput).size / 1024).toFixed(2);
  console.log(`Combined: ${finalOutput} (${finalKB} KB)`);

  const headroom = checkFlashHeadroom(root, finalOutput);
  console.log('\n' + formatFlashHeadroom(headroom));
  if (headroom.status === 'FAIL') {
    throw new Error('Flash headroom check failed (see above).');
  }
}

console.log(`\n=== BUILD COMPLETE (${target}) ===`);
if (!skipCombine) console.log(`Output: ${finalOutput}`);

if (keepWork || skipCombine) {
  console.log(`Build files kept in ${runRoot}`);
} else {
  fs.rmSync(runRoot, { recursive: true, force: true });
}
