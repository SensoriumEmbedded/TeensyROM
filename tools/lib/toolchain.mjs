// Locates arduino-cli: environment override, then PATH, then (Windows only, matching the
// pin already in Build-DualBoot.ps1) a checksummed download. arduino-cli ships via package
// managers on macOS and Linux (brew, apt, etc.), so those platforms are expected to have it
// on PATH already; we don't invent unverified checksums for those installers here.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { spawnSync } from 'node:child_process';

const WINDOWS_PIN = {
  version: '1.4.1',
  url: 'https://github.com/arduino/arduino-cli/releases/download/v1.4.1/arduino-cli_1.4.1_Windows_64bit.zip',
  sha256: '44f506a29d134cb294898d5f729aea85e5498f5d81ff5fc63c549087c45a20a3',
};

function commandExists(exe) {
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

  const cachedExe = path.join(cacheDir, 'arduino-cli.exe');
  if (fs.existsSync(cachedExe)) return cachedExe;

  if (process.platform !== 'win32') {
    throw new Error(
      'arduino-cli not found on PATH. Install it (e.g. `brew install arduino-cli` or your package manager) ' +
      'or set ARDUINO_CLI to its path.',
    );
  }

  fs.mkdirSync(cacheDir, { recursive: true });
  const zipPath = path.join(cacheDir, 'arduino-cli.zip');
  console.log(`Downloading arduino-cli v${WINDOWS_PIN.version}...`);
  const result = spawnSync('curl', ['-L', '-o', zipPath, WINDOWS_PIN.url], { stdio: 'inherit' });
  if (result.status !== 0) throw new Error('Failed to download arduino-cli');

  const actual = sha256(fs.readFileSync(zipPath));
  if (actual !== WINDOWS_PIN.sha256) {
    fs.rmSync(zipPath, { force: true });
    throw new Error(`arduino-cli checksum mismatch\n  expected: ${WINDOWS_PIN.sha256}\n  actual:   ${actual}`);
  }

  const unzip = spawnSync('tar', ['-xf', zipPath, '-C', cacheDir], { stdio: 'inherit' });
  if (unzip.status !== 0) throw new Error('Failed to extract arduino-cli');
  fs.rmSync(zipPath, { force: true });
  if (!fs.existsSync(cachedExe)) throw new Error('arduino-cli.exe missing after extraction');
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
