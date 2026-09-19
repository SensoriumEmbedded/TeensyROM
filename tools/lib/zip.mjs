// SPDX-License-Identifier: MIT
//
// A small .zip extractor on node:zlib, so downloaded tools unpack the same way on every OS
// without depending on an `unzip` binary. (`tar -xf` reads zip on Windows and macOS, where
// tar is bsdtar, but not on Linux, where it's GNU tar.) Handles what tool releases use:
// stored and deflated entries, no encryption, no ZIP64. Every entry's CRC-32 is checked.
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';

const EOCD_SIG = 0x06054b50;
const CENTRAL_SIG = 0x02014b50;
const LOCAL_SIG = 0x04034b50;

// The archive's entries: [{ name, method, crc, compressedSize, size, offset, mode }]
export function listZip(buf) {
  // The end-of-central-directory record is the last thing in the file, followed only by an
  // optional comment of up to 64 KiB, so scan backwards for it.
  let eocd = -1;
  for (let i = buf.length - 22; i >= Math.max(0, buf.length - 22 - 0xffff); i--) {
    if (buf.readUInt32LE(i) === EOCD_SIG) {
      eocd = i;
      break;
    }
  }
  if (eocd < 0) throw new Error('not a zip file (no end-of-central-directory record)');

  const count = buf.readUInt16LE(eocd + 10);
  let p = buf.readUInt32LE(eocd + 16);
  if (count === 0xffff || p === 0xffffffff) throw new Error('ZIP64 archives are not supported');

  const entries = [];
  for (let n = 0; n < count; n++) {
    if (buf.readUInt32LE(p) !== CENTRAL_SIG) throw new Error('corrupt zip: bad central directory entry');
    const madeByHost = buf[p + 5];
    const nameLength = buf.readUInt16LE(p + 28);
    const extraLength = buf.readUInt16LE(p + 30);
    const commentLength = buf.readUInt16LE(p + 32);
    const externalAttrs = buf.readUInt32LE(p + 38);
    entries.push({
      name: buf.toString('utf8', p + 46, p + 46 + nameLength),
      method: buf.readUInt16LE(p + 10),
      crc: buf.readUInt32LE(p + 16),
      compressedSize: buf.readUInt32LE(p + 20),
      size: buf.readUInt32LE(p + 24),
      offset: buf.readUInt32LE(p + 42),
      // The high 16 bits are the Unix mode when the archive was made on Unix (host 3).
      mode: madeByHost === 3 ? externalAttrs >>> 16 : 0,
    });
    p += 46 + nameLength + extraLength + commentLength;
  }
  return entries;
}

function entryData(buf, entry) {
  if (buf.readUInt32LE(entry.offset) !== LOCAL_SIG) throw new Error(`corrupt zip: bad local header for ${entry.name}`);
  const start = entry.offset + 30 + buf.readUInt16LE(entry.offset + 26) + buf.readUInt16LE(entry.offset + 28);
  const packed = buf.subarray(start, start + entry.compressedSize);
  let data;
  if (entry.method === 0) data = packed;
  else if (entry.method === 8) data = zlib.inflateRawSync(packed);
  else throw new Error(`${entry.name}: unsupported zip compression method ${entry.method}`);
  if (data.length !== entry.size || zlib.crc32(data) !== entry.crc) {
    throw new Error(`${entry.name}: zip entry is corrupt (size or CRC-32 mismatch)`);
  }
  return data;
}

// Extracts into destDir. `filter(name)` (optional) selects entries by their in-archive name.
// Returns the extracted file paths. Refuses any entry that would land outside destDir.
export function extractZip(zipPath, destDir, filter = () => true) {
  const buf = fs.readFileSync(zipPath);
  const root = path.resolve(destDir);
  const written = [];
  for (const entry of listZip(buf)) {
    if (!filter(entry.name)) continue;
    const target = path.resolve(root, entry.name);
    if (target !== root && !target.startsWith(root + path.sep)) {
      throw new Error(`refusing zip entry outside the destination: ${entry.name}`);
    }
    if (entry.name.endsWith('/')) {
      fs.mkdirSync(target, { recursive: true });
      continue;
    }
    if ((entry.mode & 0o170000) === 0o120000) throw new Error(`${entry.name}: symbolic links in zip files are not supported`);
    fs.mkdirSync(path.dirname(target), { recursive: true });
    fs.writeFileSync(target, entryData(buf, entry));
    if (entry.mode & 0o111) fs.chmodSync(target, 0o755);
    written.push(target);
  }
  return written;
}
