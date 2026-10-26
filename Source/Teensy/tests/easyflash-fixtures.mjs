// SPDX-License-Identifier: MIT
// Original bank-identification data, with no game, ROM or copyrighted assets.
// These are loader fixtures, not a runnable C64 test application.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { pathToFileURL } from 'node:url';

export function makeEasyFlashFixture({ mib = 2, sparse = false, reverse = false } = {}) {
  assert([1, 2, 4].includes(mib), 'Capacity must be 1, 2 or 4 MiB');
  const header = Buffer.alloc(64);
  header.write('C64 CARTRIDGE   '); header.writeUInt32BE(64, 16);
  header.writeUInt16BE(0x100, 20); header.writeUInt16BE(32, 22); header[24] = 1;
  header.write(`EF ${mib}M BANK IDENTIFIERS`, 32);
  const banks = mib * 64, packets = [];
  for (let bank = 0; bank < banks; ++bank) for (let half = 0; half < 2; ++half) {
    if (sparse && bank !== 0 && bank !== banks - 1 && (half ? bank % 7 === 3 : bank % 11 === 5)) continue;
    const packet = Buffer.alloc(16 + 8192);
    packet.write('CHIP'); packet.writeUInt32BE(packet.length, 4); packet.writeUInt16BE(2, 8);
    packet.writeUInt16BE(bank, 10); packet.writeUInt16BE(half ? 0xa000 : 0x8000, 12); packet.writeUInt16BE(8192, 14);
    for (let offset = 0; offset < 8192; ++offset) packet[16 + offset] = bank * 11 + half * 0x59 + offset * 7 + (offset >> 8);
    packet[16] = bank; packet[17] = half; packet[18] = bank ^ 0x5a;
    packet.write(`BANK ${String(bank).padStart(3, '0')} ROM${half ? 'H' : 'L'}`, 32);
    packets.push(packet);
  }
  return Buffer.concat([header, ...(reverse ? packets.reverse() : packets)]);
}

if (process.argv[1] && pathToFileURL(path.resolve(process.argv[1])).href === import.meta.url) {
  const options = {}, args = process.argv.slice(2);
  while (args.length) {
    const arg = args.shift();
    if (arg === '--sparse') options.sparse = true;
    else if (arg === '--reverse') options.reverse = true;
    else if (arg === '--mib' && args.length) options.mib = Number(args.shift());
    else if (arg === '--output' && args.length) options.output = path.resolve(args.shift());
    else throw Error('Use --mib 2|4 --output fixture.crt [--sparse] [--reverse]');
  }
  assert(options.output, 'Supply --output; existing files are never overwritten');
  assert([2, 4].includes(options.mib), 'The public fixture command takes --mib 2 or --mib 4');
  const bytes = makeEasyFlashFixture(options);
  fs.writeFileSync(options.output, bytes, { flag: 'wx' });
  console.log(JSON.stringify({ file: options.output, bytes: bytes.length,
    sha256: crypto.createHash('sha256').update(bytes).digest('hex'),
    banks: options.mib * 64, packets: (bytes.length - 64) / 8208,
    sparse: !!options.sparse, reverse: !!options.reverse,
    scope: 'bank-identification loader data; not a runnable C64 application' }, null, 2));
}
