// SPDX-License-Identifier: MIT
//
// Locates the tools the C64 build needs. Each one is resolved the same way as arduino-cli
// (tools/lib/toolchain.mjs): an environment override, then PATH, then a checksummed download
// pinned per platform, kept in a versioned cache directory (tools/.cache).
//
//   ACME 0.97        ACME=<path to acme>            every C64 program except TRCustomBasicCommands
//   KickAssembler    KICKASS_JAR=<path to KickAss.jar>   TRCustomBasicCommands only
//   Java             JAVA_HOME, else `java` on PATH      KickAssembler runs on it; never downloaded
//
// A tool is only resolved when a selected project needs it, so building one ACME program
// needs nothing but Node.
import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { commandExists } from './toolchain.mjs';
import { downloadVerified } from './download.mjs';
import { extractZip } from './zip.mjs';

export const ACME_VERSION = '0.97';
export const KICKASS_VERSION = '5.25';

// ACME publishes prebuilt binaries for Windows and macOS only; on Linux use your package
// manager's `acme` (or build it) and it is found on PATH. The macOS binary is x86_64, so
// Apple silicon runs it under Rosetta; Windows on ARM likewise runs the x64 build.
const ACME_WIN = {
  url: `https://sourceforge.net/projects/acme-crossass/files/win32/acme${ACME_VERSION}win.zip/download`,
  sha256: '68f7c80c23806eced6ab96622d8e22b500ed76b4d34a01af33461dee04edc359',
  exe: `acme${ACME_VERSION}win/acme/acme.exe`,
};
const ACME_MAC = {
  url: `https://sourceforge.net/projects/acme-crossass/files/macOS/acme${ACME_VERSION}mac.zip/download`,
  sha256: 'd0a9311f2e1fc63b13bc321956696f0b127c0f0fa260d75571b8e90b126c6354',
  exe: 'acme',
};
const ACME_PINS = {
  'win32-x64': ACME_WIN,
  'win32-arm64': ACME_WIN,
  'darwin-x64': ACME_MAC,
  'darwin-arm64': ACME_MAC,
};

// KickAssembler is only published at this one unversioned URL, so the checksum will stop
// matching the day the author releases a new version. That fails loudly (see the hint) and
// KICKASS_JAR keeps working in the meantime; 5.25 has been current since 2022-11.
const KICKASS = {
  url: 'https://theweb.dk/KickAssembler/KickAssembler.zip',
  sha256: '9ef752c12b25f64b55bafecfabf23dfc90a8216b1a293548b2147ed7c15f55a6',
  jar: 'KickAss.jar',
};

function fromEnv(name, what) {
  const value = process.env[name];
  if (!value) return null;
  if (!fs.existsSync(value)) throw new Error(`${name} points at a missing file: ${value}`);
  console.error(`Using ${what} from ${name}: ${value}`);
  return value;
}

// Downloads a zip, extracts the wanted entries into `<cacheDir>/<versionedName>/`, and
// returns that directory. Extracting into a scratch directory and renaming it into place
// means a run killed half way never leaves a partial tool for a later run to trust.
function fetchZipTool({ cacheDir, versionedName, pin, label, filter, mismatchHint }) {
  const finalDir = path.join(cacheDir, versionedName);
  if (fs.existsSync(finalDir)) return finalDir;

  fs.mkdirSync(cacheDir, { recursive: true });
  const zipPath = path.join(cacheDir, `${versionedName}.zip`);
  downloadVerified({ url: pin.url, sha256: pin.sha256, dest: zipPath, label, mismatchHint });
  const scratch = fs.mkdtempSync(path.join(cacheDir, '.extract-'));
  try {
    extractZip(zipPath, scratch, filter);
    fs.renameSync(scratch, finalDir);
  } finally {
    fs.rmSync(scratch, { recursive: true, force: true });
    fs.rmSync(zipPath, { force: true });
  }
  return finalDir;
}

// An ACME other than the pinned one assembles the same sources into different bytes, and
// those bytes are committed as headers -- so say so rather than let the diff be a surprise.
// A warning, not an error: on Linux the only sensible source is the distro package, and
// which release that is depends on the distro.
function warnUnlessPinnedAcme(acme) {
  const probe = spawnSync(acme, ['--version'], { encoding: 'utf8' });
  const found = `${probe.stdout ?? ''}${probe.stderr ?? ''}`.match(/release\s+([\d.]+)/)?.[1];
  if (found !== ACME_VERSION) {
    console.error(
      `Warning: using ACME ${found ?? '(unknown version)'} from ${acme}, but this build is pinned to ${ACME_VERSION}. ` +
      `The generated headers may differ from the committed ones; set ACME to a ${ACME_VERSION} binary to be sure.`,
    );
  }
  return acme;
}

// Returns a path (or the bare name `acme`, when it's on PATH) to run.
export function resolveAcme(cacheDir) {
  const override = fromEnv('ACME', 'ACME');
  if (override) return warnUnlessPinnedAcme(override);
  if (commandExists('acme')) return warnUnlessPinnedAcme('acme');

  const platform = `${process.platform}-${process.arch}`;
  const pin = ACME_PINS[platform];
  if (!pin) {
    throw new Error(
      `acme not found on PATH, and there is no pinned download for ${platform}. ` +
      `Install ACME ${ACME_VERSION} (for example \`apt install acme\` or \`brew install acme\`) or set ACME to its path.`,
    );
  }
  const dir = fetchZipTool({
    cacheDir,
    // The platform is part of the cache name because the pins are per-platform and the
    // archives have different layouts; a tools/.cache shared between, say, a macOS checkout
    // and a Windows VM would otherwise hand the second one the first one's extracted tree.
    versionedName: `acme-${ACME_VERSION}-${platform}`,
    pin,
    label: `ACME ${ACME_VERSION} for ${platform}`,
  });
  const exe = path.join(dir, pin.exe);
  if (!fs.existsSync(exe)) throw new Error(`ACME archive did not contain ${pin.exe}`);
  return exe;
}

// Returns the path to KickAss.jar.
export function resolveKickAssembler(cacheDir) {
  const override = fromEnv('KICKASS_JAR', 'KickAssembler');
  if (override) return override;

  const dir = fetchZipTool({
    cacheDir,
    versionedName: `kickassembler-${KICKASS_VERSION}`,
    pin: KICKASS,
    label: `KickAssembler ${KICKASS_VERSION}`,
    filter: (name) => name === KICKASS.jar,
    mismatchHint:
      'KickAssembler is only published at an unversioned URL, so this most likely means a new version has been released.\n' +
      'Download it yourself and set KICKASS_JAR to its KickAss.jar; to pin the new version, update KICKASS in tools/lib/c64-toolchain.mjs.',
  });
  const jar = path.join(dir, KICKASS.jar);
  if (!fs.existsSync(jar)) throw new Error(`KickAssembler archive did not contain ${KICKASS.jar}`);
  return jar;
}

// Returns the java command to run. Checked by running it: on macOS a `java` stub exists on
// PATH even when no JDK is installed, and it fails when run.
export function resolveJava() {
  let java = 'java';
  if (process.env.JAVA_HOME) {
    java = path.join(process.env.JAVA_HOME, 'bin', process.platform === 'win32' ? 'java.exe' : 'java');
    if (!fs.existsSync(java)) throw new Error(`JAVA_HOME does not contain a Java runtime: ${java} is missing`);
  } else if (!commandExists('java')) {
    throw new Error('Java not found. KickAssembler (used only by TRCustomBasicCommands) needs a Java runtime, version 8 or newer: install one, or set JAVA_HOME.');
  }
  if (spawnSync(java, ['-version'], { stdio: 'ignore' }).status !== 0) {
    throw new Error(`\`${java} -version\` failed, so there is no working Java runtime. Install one, or set JAVA_HOME.`);
  }
  return java;
}
