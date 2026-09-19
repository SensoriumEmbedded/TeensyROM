// SPDX-License-Identifier: MIT
//
// Node port of Source/Teensy/tools/Build-DualBoot.ps1. Builds the dual-boot TeensyROM
// image (MinimalBoot at 0x60000000 + the main image at 0x60060000, combined into one hex)
// for one of two release targets:
//
//   --target tr        plain TeensyROM (Fab 0.2/0.3)
//   --target tr-plus   TeensyROM+ (Fab 0.4). Always includes MPE once MPE is on main; until
//                       then this builds today's TR+ image under today's name.
//
// Unlike Build-DualBoot.ps1, this never touches the installed Teensy core: the boot/linker
// file swap happens in a private copy made fresh for this run, so nothing about your
// toolchain changes because you ran a build. See the "installed core unchanged" check below.
//
// The original script's two Read-Host prompts become flags, so a non-interactive run fails
// fast instead of hanging:
//   --yes    confirm disabling an active Fab04_Features #define when building --target tr
//   --force  overwrite an existing output file
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

const TEENSY_CORE_VERSION = '1.61.0';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const args = process.argv.slice(2);

function flag(name) {
  return args.includes(name);
}
function option(name, fallback) {
  const i = args.indexOf(name);
  if (i < 0) return fallback;
  if (!args[i + 1] || args[i + 1].startsWith('--')) throw new Error(`Missing value for ${name}`);
  return args[i + 1];
}

const target = option('--target', null);
if (!['tr', 'tr-plus'].includes(target)) {
  throw new Error('Use --target tr or --target tr-plus');
}
const fab04Features = target === 'tr-plus';
const yes = flag('--yes');
const force = flag('--force');
const skipTeensyBuild = flag('--skip-teensy-build');
const skipMinimalBuild = flag('--skip-minimal-build');
const skipCombine = flag('--skip-combine');
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
const fab04Active = /^\s*#define\s+Fab04_Features\b/m.test(read(fab04CtlPath));
if (!fab04Features && fab04Active) {
  if (!yes) {
    throw new Error(
      `Fab04_Features is #define'd in ${fab04CtlPath}, but --target tr was requested.\n` +
      'This mismatch would build a TR+ image while labeled as plain TR.\n' +
      'Comment out the #define yourself, re-run with --target tr-plus, or pass --yes to have this comment it out and continue.',
    );
  }
  const updated = read(fab04CtlPath).replace(/^(\s*)#define(\s+Fab04_Features\b.*)$/m, '$1// #define$2');
  fs.writeFileSync(fab04CtlPath, updated);
  console.log(`Fab04_Features #define commented out in ${fab04CtlPath}`);
}

// --- Output path ---
const finalOutput = path.join(outDir, target === 'tr-plus' ? `TeensyROM+_${trVersion}_full.hex` : `TeensyROM_${trVersion}_full.hex`);
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

function copyLinkerFiles(suffix) {
  fs.copyFileSync(path.join(linkers, `bootdata.c.${suffix}`), path.join(privateCore, 'bootdata.c'));
  fs.copyFileSync(path.join(linkers, `imxrt1062_t41.ld.${suffix}`), path.join(privateCore, 'imxrt1062_t41.ld'));
}

function build(name, { inoPath, fqbn, suffix, elfStem }) {
  console.log(`\n[${name}] Building`);
  copyLinkerFiles(suffix);
  const buildDir = path.join(runRoot, name);
  const props = run(cli, ['compile', '--fqbn', fqbn, '--build-path', buildDir, ...compilerPathProps, '--show-properties', inoPath], env);
  const defsMatch = props.match(/^build\.flags\.defs=(.*)$/m);
  if (!defsMatch) throw new Error('Could not retrieve build.flags.defs from --show-properties');
  const defs = defsMatch[1].trim() + (fab04Features ? ' -DFab04_Features' : '');

  const log = run(cli, ['compile', '--fqbn', fqbn, '--build-path', buildDir, ...compilerPathProps, '--build-property', `build.flags.defs=${defs}`, inoPath], env);
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
  const sizes = run(armBin + 'size' + exeSuffix, ['-A', elf], env);
  write(path.join(runRoot, `${name}.size`), sizes);

  return { name, elf, hex };
}

let minimalImage = null, teensyImage = null;

if (!skipMinimalBuild) {
  minimalImage = build('minimal', {
    inoPath: path.join(root, 'Source/Teensy/MinimalBoot/MinimalBoot.ino'),
    fqbn: 'teensy:avr:teensy41:usb=serial,speed=600,opt=o2std,keys=en-us',
    suffix: 'orig',
    elfStem: 'MinimalBoot',
  });
}

if (!skipTeensyBuild) {
  teensyImage = build('main', {
    inoPath: path.join(root, 'Source/Teensy/Teensy.ino'),
    fqbn: 'teensy:avr:teensy41:usb=serialmidi,speed=600,opt=o2std,keys=en-us',
    suffix: 'upper',
    elfStem: 'Teensy',
  });
}

// The private copy is the only thing the linker-file swap touched. Confirm the fix holds.
const installedCoreHashAfter = hashGuardedFiles();
if (installedCoreHashBefore !== installedCoreHashAfter) {
  throw new Error(`Installed Teensy core changed during the build (${guardedFiles.join(', ')}). This should be impossible with the private-copy build.`);
}
console.log('\nInstalled Teensy core unchanged (bootdata.c, imxrt1062_t41.ld hash match before/after).');

if (!skipCombine) {
  if (!minimalImage || !teensyImage) throw new Error('Combining requires both images; pass --skip-combine yourself if that is intentional');
  console.log('\n[combine] Combining hex images');
  const combined = legacyCombineHex(read(minimalImage.hex), read(teensyImage.hex));
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
