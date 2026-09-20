// SPDX-License-Identifier: MIT
//
// Downloads a file and checks it against a pinned SHA-256 before anything uses it. A
// mismatch deletes the download and fails, so a changed or tampered file is never trusted.
// (tools/lib/toolchain.mjs does the same inline for arduino-cli.)
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { spawnSync } from 'node:child_process';

export function sha256File(file) {
  return crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
}

// label: what to call it in messages. mismatchHint: extra advice for the checksum error.
export function downloadVerified({ url, sha256, dest, label, mismatchHint = '' }) {
  fs.mkdirSync(path.dirname(dest), { recursive: true });
  console.error(`Downloading ${label}...`);
  const result = spawnSync('curl', ['-fsSL', '--retry', '3', '--retry-connrefused', '-o', dest, url], { stdio: 'inherit' });
  if (result.status !== 0) {
    fs.rmSync(dest, { force: true });
    throw new Error(`Failed to download ${label} from ${url}`);
  }
  const actual = sha256File(dest);
  if (actual !== sha256) {
    fs.rmSync(dest, { force: true });
    throw new Error(
      `${label}: checksum mismatch\n  expected: ${sha256}\n  actual:   ${actual}` +
      (mismatchHint ? `\n${mismatchHint}` : ''),
    );
  }
}
