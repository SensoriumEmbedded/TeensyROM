// SPDX-License-Identifier: MIT
//
// Packages an extension host image into the .TRH container the device installs from.
//
//   node tools/build-host-package.mjs --hex build/firmware/TeensyROM+_0.8.0.11_full.hex
//   node tools/build-host-package.mjs --image my-host.bin --out MYHOST.TRH
//
// Two inputs, because there are two people doing this. --hex takes a combined TR+
// firmware hex and lifts the extension image back out of its flash slot: that is the
// host this firmware ships with, packaged so it can be installed onto another board
// without reflashing it. --image takes the raw `objcopy -O binary` output a host
// author has in hand before there is any firmware around it.
//
// Every check the device applies before it erases is applied here, against the same
// constants, so a package that reaches the C64 has already been refused on the host
// side if it was going to be refused at all. The one thing this cannot check is
// whether the image is the host you meant, so it prints the descriptor it found.
import fs from 'node:fs';
import path from 'node:path';
import { decodeHex, VM_BASE, VM_LIMIT } from './lib/hex.mjs';
import { buildHostPackage, parseHostPackage, hostDescriptor } from './lib/extension.mjs';

const args = process.argv.slice(2);
function option(name, fallback = null) {
  const i = args.indexOf(name);
  if (i < 0) return fallback;
  if (!args[i + 1] || args[i + 1].startsWith('--')) throw new Error(`Missing value for ${name}`);
  return args[i + 1];
}

// The slot is flash: what the hex does not mention is erased, which reads as 0xFF. A hole
// in the middle means the image was linked with one, not that bytes went missing, so it is
// packaged as the part would hold it rather than refused.
export function hostImageFromHex(text) {
  const bytes = decodeHex(text);
  const present = [...bytes.keys()].filter((a) => a >= VM_BASE && a < VM_LIMIT);
  if (!present.length) {
    throw new Error('This hex carries nothing in the extension slot. Build it with ' +
      '--target tr-plus (and without --no-extensions) if you wanted a host in it.');
  }
  const top = Math.max(...present);
  const image = Buffer.alloc(top - VM_BASE + 1, 0xff);
  for (const address of present) image[address - VM_BASE] = bytes.get(address);
  return image;
}

function main() {
  const hexPath = option('--hex');
  const imagePath = option('--image');
  if (!hexPath === !imagePath) {
    throw new Error('Pass exactly one of --hex <firmware.hex> or --image <host.bin>');
  }

  const image = hexPath
    ? hostImageFromHex(fs.readFileSync(hexPath, 'utf8'))
    : fs.readFileSync(imagePath);

  const pkg = buildHostPackage({ image });
  // Read our own output back through the device's reader before writing it anywhere, so a
  // packaging bug fails here rather than on a C64 with the slot already erased.
  const header = parseHostPackage(pkg);
  const id = hostDescriptor(pkg.subarray(header.headerBytes));

  const source = hexPath ?? imagePath;
  const outPath = path.resolve(option('--out',
    path.join(path.dirname(source), `${(id.name || 'HOST').toUpperCase()}.TRH`)));
  fs.writeFileSync(outPath, pkg);

  console.log(`Host "${id.name}" from ${source}`);
  console.log(`  ABI ${id.abi}, services 0x${id.services.toString(16).padStart(8, '0')}`);
  console.log(`  payload ${header.payloadBytes} bytes, entry 0x${header.entry.toString(16)}`);
  console.log(`  wrote ${outPath} (${pkg.length} bytes)`);
  console.log('Copy it to the card and select it in the menu, or push it over USB with ' +
    'tools/bench/hostinstall.py.');
}

if (import.meta.url === `file://${process.argv[1]}`) {
  try {
    main();
  } catch (error) {
    console.error(`build-host-package: ${error.message}`);
    process.exit(1);
  }
}
