// Locates arduino-cli: environment override, then PATH, then a checksummed download pinned
// per platform/arch. We used to trust package managers (brew, apt) or the arduino/setup-
// arduino-cli GitHub Action for this, but the action still declares `using: "node20"` with
// no newer release to move to, so CI now goes through this same resolver instead of the
// action. One pinned version governs every OS.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { spawnSync } from 'node:child_process';

const PIN_VERSION = '1.5.1';

// Checksums copied verbatim from the release's own `<version>-checksums.txt`.
const PINS = {
  'win32-x64': {
    url: `https://github.com/arduino/arduino-cli/releases/download/v${PIN_VERSION}/arduino-cli_${PIN_VERSION}_Windows_64bit.zip`,
    sha256: 'fabe42e0eb04d00e776a66178299ff95a46c623dbc260f997e58fd514853dd40',
    archive: 'zip',
    exe: 'arduino-cli.exe',
  },
  'linux-x64': {
    url: `https://github.com/arduino/arduino-cli/releases/download/v${PIN_VERSION}/arduino-cli_${PIN_VERSION}_Linux_64bit.tar.gz`,
    sha256: '28a8e119c498a25607821c36cb2dc49e8463941b261a0d99091baa7bc692dd2b',
    archive: 'tar',
    exe: 'arduino-cli',
  },
  'darwin-x64': {
    url: `https://github.com/arduino/arduino-cli/releases/download/v${PIN_VERSION}/arduino-cli_${PIN_VERSION}_macOS_64bit.tar.gz`,
    sha256: 'c982e940027996bea9901050e95fae99c59c1dcfee54beedecaf28141e7bf2e7',
    archive: 'tar',
    exe: 'arduino-cli',
  },
  'darwin-arm64': {
    url: `https://github.com/arduino/arduino-cli/releases/download/v${PIN_VERSION}/arduino-cli_${PIN_VERSION}_macOS_ARM64.tar.gz`,
    sha256: 'cb952e8c1621c95ef5f1d17831c945e3d0ec5973f89c557a7ec8feb9c4f7d4c9',
    archive: 'tar',
    exe: 'arduino-cli',
  },
};

export function commandExists(exe) {
  const probe = process.platform === 'win32' ? spawnSync('where', [exe]) : spawnSync('which', [exe]);
  return probe.status === 0;
}

function sha256(buffer) {
  return crypto.createHash('sha256').update(buffer).digest('hex');
}

// cacheDir: where a downloaded copy is kept between runs (e.g. tools/.cache).
export function resolveArduinoCli(cacheDir) {
  const fromEnv = process.env.ARDUINO_CLI;
  if (fromEnv) {
    if (!fs.existsSync(fromEnv)) throw new Error(`ARDUINO_CLI points at a missing file: ${fromEnv}`);
    return fromEnv;
  }

  if (commandExists('arduino-cli')) return 'arduino-cli';

  const pin = PINS[`${process.platform}-${process.arch}`];
  if (!pin) {
    throw new Error(
      `arduino-cli not found on PATH, and no pinned download exists for ${process.platform}-${process.arch}. ` +
      'Install it yourself or set ARDUINO_CLI to its path.',
    );
  }

  // The version is part of the cache path so bumping PIN_VERSION can't silently keep
  // serving an old binary from a dev machine's populated cache; it just adds a new one.
  const versionedCacheDir = path.join(cacheDir, PIN_VERSION);
  const cachedExe = path.join(versionedCacheDir, pin.exe);
  if (fs.existsSync(cachedExe)) return cachedExe;

  fs.mkdirSync(versionedCacheDir, { recursive: true });
  const archivePath = path.join(versionedCacheDir, pin.archive === 'zip' ? 'arduino-cli.zip' : 'arduino-cli.tar.gz');
  console.error(`Downloading arduino-cli v${PIN_VERSION} for ${process.platform}-${process.arch}...`);
  const result = spawnSync('curl', ['-fL', '--retry', '3', '--retry-connrefused', '-o', archivePath, pin.url], { stdio: 'inherit' });
  if (result.status !== 0) throw new Error('Failed to download arduino-cli');

  const actual = sha256(fs.readFileSync(archivePath));
  if (actual !== pin.sha256) {
    fs.rmSync(archivePath, { force: true });
    throw new Error(`arduino-cli checksum mismatch\n  expected: ${pin.sha256}\n  actual:   ${actual}`);
  }

  // Extract into a scratch dir and rename into place, so a run killed mid-extract never
  // leaves a partial (or non-executable) binary at cachedExe for a later run to trust.
  const scratchDir = fs.mkdtempSync(path.join(versionedCacheDir, '.extract-'));
  const extractArgs = pin.archive === 'zip' ? ['-xf', archivePath, '-C', scratchDir] : ['-xzf', archivePath, '-C', scratchDir];
  const extract = spawnSync('tar', extractArgs, { stdio: 'inherit' });
  fs.rmSync(archivePath, { force: true });
  const scratchExe = path.join(scratchDir, pin.exe);
  if (extract.status !== 0 || !fs.existsSync(scratchExe)) {
    fs.rmSync(scratchDir, { recursive: true, force: true });
    throw new Error('Failed to extract arduino-cli');
  }
  if (pin.archive !== 'zip') fs.chmodSync(scratchExe, 0o755);
  fs.renameSync(scratchExe, cachedExe);
  fs.rmSync(scratchDir, { recursive: true, force: true });
  return cachedExe;
}

// Default Arduino data directory per OS, matching arduino-cli's own defaults.
export function defaultArduinoDataDir() {
  if (process.env.ARDUINO_DIRECTORIES_DATA) return process.env.ARDUINO_DIRECTORIES_DATA;
  if (process.platform === 'win32') {
    const base = process.env.LOCALAPPDATA;
    if (!base) throw new Error('LOCALAPPDATA is not set');
    const primary = path.join(base, 'Arduino15');
    return fs.existsSync(primary) ? primary : path.join(base, '.arduino15');
  }
  if (process.platform === 'darwin') {
    return path.join(process.env.HOME, 'Library', 'Arduino15');
  }
  return path.join(process.env.HOME, '.arduino15');
}

// arduino-cli's own default sketchbook path (`directories.user`), which is *not* the same
// on every platform: Windows and macOS use Documents/Arduino, but Linux just uses ~/Arduino.
// `arduino-cli lib install` (run separately, with no override) lands libraries here, so this
// has to match arduino-cli's real default exactly or a step like the workflow's
// `arduino-cli lib install CRC32` silently installs somewhere the build never looks.
export function defaultArduinoUserDir() {
  if (process.env.ARDUINO_DIRECTORIES_USER) return process.env.ARDUINO_DIRECTORIES_USER;
  if (process.platform === 'win32') {
    return path.join(process.env.USERPROFILE ?? process.env.HOME, 'Documents', 'Arduino');
  }
  if (process.platform === 'darwin') {
    return path.join(process.env.HOME, 'Documents', 'Arduino');
  }
  return path.join(process.env.HOME, 'Arduino');
}
