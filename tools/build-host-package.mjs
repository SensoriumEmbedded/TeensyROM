// SPDX-License-Identifier: MIT
//
// Packages an extension host image into the .TRH container the device installs from.
//
//   node tools/build-host-package.mjs --hex my-host.hex --out MYHOST.TRH
//   node tools/build-host-package.mjs --image my-host.bin --out MYHOST.TRH
//
// build-firmware.mjs already packages every host it builds, the stock one included, so
// this is for a host that came out of some other build. Two inputs, because there are
// two ways to have one. --hex takes a hex linked for the extension slot and lifts the
// slot's bytes out of it, ignoring anything else it holds. --image takes the raw
// `objcopy -O binary` output a host author has in hand without any hex around it.
//
// A firmware hex is not an input: no firmware carries a host, and this refuses one for
// holding nothing in the slot. CI does not lean on that refusal to prove a release hex is
// host-free -- it would also refuse a slot holding a host it cannot package -- and decodes
// the hex itself instead (.github/workflows/build.yml).
//
// Every check the device applies before it erases is applied here, against the same
// constants, so a package that reaches the C64 has already been refused on the host
// side if it was going to be refused at all. The one thing this cannot check is
// whether the image is the host you meant, so it prints the descriptor it found.
//
// Without --out the file is named after that descriptor, reduced to filename characters
// first: the name is twelve bytes the host author chose and is not a path, however much
// "../../pwned" looks like one.
import fs from 'node:fs';
import path from 'node:path';
import { decodeHex, VM_BASE, VM_LIMIT } from './lib/hex.mjs';
import { scanArgs } from './lib/cli-args.mjs';
import { buildHostPackage, parseHostPackage, hostDescriptor,
         hostFileStem, hostNameForDisplay } from './lib/extension.mjs';

// Scanned inside main(), not at module scope: this file also exports hostImageFromHex, and
// a top-level scan would run against whatever argv the importing process happens to have --
// which under `node --test` is the runner's own arguments, and they are not ours to know.
const KNOWN = { options: ['--hex', '--image', '--out'] };

// The slot is flash: what the hex does not mention is erased, which reads as 0xFF. A hole
// in the middle means the image was linked with one, not that bytes went missing, so it is
// packaged as the part would hold it rather than refused.
export function hostImageFromHex(text) {
  const bytes = decodeHex(text);
  // Walked rather than spread into Math.max. Math.max(...present) passes one argument per
  // byte present, which crosses V8's argument limit somewhere above 120 KiB -- well inside
  // a 384 KiB slot, and the example host is already past 80 KiB. It fails as "Maximum call
  // stack size exceeded", which names nothing, and CI runs this on every tr-plus build, so
  // the first sight of it would be an opaque failure on a hex that packages fine locally.
  let top = -1;
  for (const address of bytes.keys()) {
    if (address >= VM_BASE && address < VM_LIMIT && address > top) top = address;
  }
  if (top < 0) {
    throw new Error('This hex carries nothing in the extension slot. A firmware hex never ' +
      'does: pass the extension image\'s own hex, or use the .TRH build-firmware.mjs wrote ' +
      'beside the firmware.');
  }
  const image = Buffer.alloc(top - VM_BASE + 1, 0xff);
  for (const [address, value] of bytes) {
    if (address >= VM_BASE && address < VM_LIMIT) image[address - VM_BASE] = value;
  }
  return image;
}

function main() {
  const { option } = scanArgs(process.argv.slice(2), KNOWN);
  const hexPath = option('--hex');
  const imagePath = option('--image');
  if (!hexPath === !imagePath) {
    throw new Error('Pass exactly one of --hex <host.hex> or --image <host.bin>');
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
  const explicitOut = option('--out');
  const stem = hostFileStem(id.name);
  const shown = hostNameForDisplay(id.name);

  let outPath;
  if (explicitOut !== null) {
    // An explicit path is the developer's own choice and is not second-guessed; CI passes
    // one. Only the name the descriptor supplies is untrusted here.
    outPath = path.resolve(explicitOut);
  } else {
    const dir = path.resolve(path.dirname(source));
    outPath = path.resolve(path.join(dir, `${stem || 'HOST'}.TRH`));
    // Checked rather than trusted to the sanitizer above: this is what still holds if the
    // character policy is ever loosened, and it is the condition the traversal broke.
    if (path.dirname(outPath) !== dir) {
      throw new Error(`Refusing to write outside ${dir}: the descriptor name ` +
        `"${shown}" does not name a file in it. Pass --out to choose the path yourself.`);
    }
    if (stem !== id.name.toUpperCase()) {
      console.log(`Note: the descriptor name "${shown}" is not a filename; ` +
        `writing ${stem || 'HOST'}.TRH rather than naming the file after it.`);
    }
  }
  fs.writeFileSync(outPath, pkg);

  console.log(`Host "${shown}" from ${source}`);
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
