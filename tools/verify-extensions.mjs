// SPDX-License-Identifier: MIT
//
// Compiles and runs the extension-loader conformance suite on the host. No
// Teensy, no ARM toolchain and no SD card: every test here compiles the real
// firmware headers and the real module source against packages written by the
// real packager, so the JavaScript writer and the C++ reader are checked
// against each other rather than each against itself.
//
// Hardware acceptance is a separate step and is not implied by a pass here.
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { hostPackageFixture, registryFixture } from './lib/fixtures.mjs';
import { ASSIGNED_SERVICES, BASE_SERVICES, RAM_BYTES, RAM_RESERVED_BYTES, RAM2_RO_BYTES,
         SERVICE } from './lib/extension.mjs';
import { VM_BASE, VM_LIMIT } from './lib/hex.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const args = process.argv.slice(2);
const option = (name, fallback) => { const i = args.indexOf(name); return i < 0 ? fallback : args[i + 1]; };
const compiler = option('--cxx', process.env.CXX ?? 'g++');
const output = fs.mkdtempSync(path.join(os.tmpdir(), 'vm-verify-'));
const keep = args.includes('--keep');

function run(exe, argv, label) {
  const result = spawnSync(exe, argv, { cwd: root, encoding: 'utf8', windowsHide: true, maxBuffer: 16 * 1024 * 1024 });
  // A failed assert() aborts, which leaves status null and the signal set --
  // so checking status alone reports an aborted C++ test as a pass.
  if (result.error || result.status || result.signal) {
    process.stderr.write(`${result.stdout ?? ''}${result.stderr ?? ''}`);
    const why = result.error ?? (result.signal ? `signal ${result.signal}` : `exit ${result.status}`);
    throw new Error(`${label} failed (${why})`);
  }
  return (result.stdout ?? '') + (result.stderr ?? '');
}

function native(name, argv = []) {
  const exe = path.join(output, name + (process.platform === 'win32' ? '.exe' : ''));
  run(compiler, ['-std=c++17', '-O2', '-Wall', '-Wextra',
    ...(process.platform === 'win32' ? ['-static'] : []),
    '-I', root, path.join(root, 'vm/tests', name + '.cpp'), '-o', exe], `compiling ${name}`);
  process.stdout.write(run(exe, argv, name));
}

const sandbox = (prefix) => fs.mkdtempSync(path.join(output, prefix));

const MODULE_ABI = 'Source/Teensy/MinimalBoot/Common/VMABI.h';
const HOST_ABI = 'Source/Teensy/MinimalBoot/Common/VMHostABI.h';

// The flash slot the extension image is linked into is written down twice: here,
// where the hex is partitioned, and in VMHostABI.h, which is what a host vendor
// compiles against and what the minimal image uses to decide whether that slot
// holds an image it may jump to. Neither side can see the other, so compare them.
function checkBootSlot() {
  const header = fs.readFileSync(path.join(root, HOST_ABI), 'utf8');
  for (const [name, expected] of [['VM_HOST_SLOT_BASE', VM_BASE], ['VM_HOST_SLOT_LIMIT', VM_LIMIT]]) {
    const match = header.match(new RegExp(`${name} = (0x[0-9a-fA-F]+)u?`));
    if (!match) throw new Error(`VMHostABI.h no longer declares ${name}`);
    if (parseInt(match[1], 16) !== expected) {
      throw new Error(`${name} is ${match[1]}, but hex.mjs partitions the extension slot at 0x${expected.toString(16)}`);
    }
  }
  console.log('PASS: VM_HOST_SLOT_BASE/LIMIT match the extension flash partition in tools/lib/hex.mjs');
}

// The host descriptor's offset is written down twice too: in the linker script
// that places .vmhostid, and in VMHostABI.h, which is where the main image and
// a host vendor both read it. A mismatch reads erased flash and silently
// disables the refusal.
function checkHostIdOffset() {
  const header = fs.readFileSync(path.join(root, HOST_ABI), 'utf8');
  const image = fs.readFileSync(path.join(root, 'tools/lib/extension-image.mjs'), 'utf8');
  const declared = header.match(/VM_HOST_ID_OFFSET = (0x[0-9a-fA-F]+)u?/);
  if (!declared) throw new Error('VMHostABI.h no longer declares VM_HOST_ID_OFFSET');
  const placed = image.match(/\. = ORIGIN\(FLASH\) \+ (0x[0-9a-fA-F]+);\s*KEEP\(\*\(\.vmhostid\)\)/);
  if (!placed) throw new Error('extensionLinkerScript no longer places .vmhostid at a fixed offset');
  if (parseInt(declared[1], 16) !== parseInt(placed[1], 16)) {
    throw new Error(`VM_HOST_ID_OFFSET is ${declared[1]}, but the linker script places .vmhostid at ${placed[1]}`);
  }
  console.log('PASS: VM_HOST_ID_OFFSET matches where extensionLinkerScript places .vmhostid');
}

// tools/lib/extension.mjs mirrors the RAM2 sizes from VMABI.h. Only
// RAM2_RO_BYTES is enforced when packaging, so nothing would catch the other
// two drifting. Nothing imports both, so compare them here.
function checkRam2Sizes() {
  const header = fs.readFileSync(path.join(root, MODULE_ABI), 'utf8');
  for (const [name, mirrored] of [['VM_RAM_BYTES', RAM_BYTES],
                                  ['VM_RAM_RESERVED_BYTES', RAM_RESERVED_BYTES],
                                  ['VM_RAM2_RO_BYTES', RAM2_RO_BYTES]]) {
    const match = header.match(new RegExp(`\\b${name}\\s*=\\s*(\\d+)\\s*\\*\\s*1024\\b`));
    if (!match) throw new Error(`VMABI.h no longer defines ${name} as a KiB multiple`);
    const declared = Number(match[1]) * 1024;
    if (declared !== mirrored) {
      throw new Error(`${name} is ${declared} in VMABI.h, but tools/lib/extension.mjs mirrors it as ${mirrored}`);
    }
  }
  console.log('PASS: tools/lib/extension.mjs RAM2 sizes match VM_RAM_* in VMABI.h');
}

// The EEPROM addresses and boot-indicator values a host needs are duplicated
// out of Common_Defs.h, which is firmware-wide and must never be included by a
// vendor. Nothing links both, so compare them.
function checkEepromProtocol() {
  const defs = fs.readFileSync(path.join(root, 'Source/Teensy/MinimalBoot/Common/Common_Defs.h'), 'utf8');
  const host = fs.readFileSync(path.join(root, HOST_ABI), 'utf8');
  const read = (text, name, pattern) => {
    const match = text.match(pattern);
    if (!match) throw new Error(`cannot read ${name} from ${text === defs ? 'Common_Defs.h' : 'VMHostABI.h'}`);
    return match[1];
  };
  const pairs = [
    ['magic', /#define eepMagicNum\s+(0x[0-9a-fA-F]+)/, /VM_EEP_MAGIC = (0x[0-9a-fA-F]+)u?/, 16],
    ['magic address', /eepAdMagicNum\s*=\s*(\d+)/, /VM_EEP_MAGIC_ADDR = (\d+)/, 10],
    ['boot name address', /eepAdCrtBootName\s*=\s*(\d+)/, /VM_EEP_BOOTNAME_ADDR = (\d+)/, 10],
    ['boot indicator address', /eepAdMinBootInd\s*=\s*(\d+)/, /VM_EEP_BOOTIND_ADDR = (\d+)/, 10],
    ['SkipMin', /MinBootInd_SkipMin\s*=\s*(\d+)/, /VM_BOOT_SKIP_MIN = (\d+)/, 10],
    ['ExecuteMin', /MinBootInd_ExecuteMin\s*=\s*(\d+)/, /VM_BOOT_EXECUTE_MIN = (\d+)/, 10],
    ['FromMin', /MinBootInd_FromMin\s*=\s*(\d+)/, /VM_BOOT_FROM_MIN = (\d+)/, 10],
  ];
  for (const [name, internal, published, radix] of pairs) {
    const a = parseInt(read(defs, name, internal), radix);
    const b = parseInt(read(host, name, published), radix);
    if (a !== b) throw new Error(`EEPROM ${name} is ${a} in Common_Defs.h but ${b} in VMHostABI.h`);
  }
  console.log('PASS: VMHostABI.h EEPROM protocol matches Common_Defs.h');
}

// The packager mirrors both the registry and the base profile, and subtracts
// one from the other to decide which numbers are still free to hand out.
// Nothing imports both copies, so compare them here.
function checkServiceRegistry() {
  const header = fs.readFileSync(path.join(root, MODULE_ABI), 'utf8');
  const orList = (name) => {
    const match = header.match(new RegExp(`\\b${name}\\s*=\\s*([0-9|x a-fA-F]+?),`));
    if (!match) throw new Error(`VMABI.h no longer defines ${name} as an or-list of literals`);
    return match[1].split('|').reduce((bits, term) => bits | Number(term.trim()), 0) >>> 0;
  };
  for (const [name, mirrored] of [['VM_SERVICES_ASSIGNED', ASSIGNED_SERVICES],
                                  ['VM_SERVICES', BASE_SERVICES],
                                  ['VM_SERVICE_RAM2_RO', SERVICE.RAM2_RO]]) {
    const declared = orList(name);
    if (declared !== mirrored) {
      throw new Error(`${name} is 0x${declared.toString(16)} in VMABI.h, but ` +
                      `tools/lib/extension.mjs mirrors it as 0x${mirrored.toString(16)}`);
    }
  }
  console.log('PASS: the service registry and base profile in tools/lib/extension.mjs match VMABI.h');
}

// A directory holding only these files, so a build inside it sees nothing else
// of TeensyROM whatever the include path would otherwise have offered.
function vendorDir(...files) {
  const dir = fs.mkdtempSync(path.join(output, 'standalone-'));
  for (const file of files) fs.copyFileSync(path.join(root, file), path.join(dir, path.basename(file)));
  return dir;
}

// Everything the published headers may include. The native build below settles
// the #if defined(__arm__) and #ifdef FLASHMEM branches the target takes the
// other way, so an include under either would compile clean there and fail
// only for the vendor; read the directives rather than the branch chosen here.
const PUBLISHED_INCLUDES = {
  'VMABI.h': ['<stdint.h>', '<stddef.h>'],
  'VMHostABI.h': ['<stdint.h>', '<stddef.h>', '<string.h>', '"VMABI.h"'],
};

function checkPublishedIncludes() {
  for (const [name, allowed] of Object.entries(PUBLISHED_INCLUDES)) {
    const text = fs.readFileSync(path.join(root, path.dirname(MODULE_ABI), name), 'utf8');
    for (const [, taken] of text.matchAll(/^[ \t]*#[ \t]*include[ \t]+(\S+)/gm)) {
      if (!allowed.includes(taken)) {
        throw new Error(`${name} includes ${taken}, which a vendor who copied it out does not have`);
      }
    }
  }
  console.log('PASS: VMABI.h and VMHostABI.h include nothing outside the published set, on every target');
}

// VMABI.h says it is the whole contract for a module, and VMHostABI.h says the
// two together are the whole contract for a host. Build each claim the way its
// vendor would -- in a directory holding nothing else, no include path back
// here -- since an include added to either still builds everywhere in this
// repository.
function checkPublishedHeadersStandalone() {
  const moduleAlone = vendorDir(MODULE_ABI);
  const moduleOnly = path.join(moduleAlone, 'module_only.cpp');
  fs.writeFileSync(moduleOnly, '#include "VMABI.h"\nint main(){return 0;}\n');
  run(compiler, ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsyntax-only', moduleOnly],
    'compiling VMABI.h with nothing beside it');
  console.log('PASS: VMABI.h builds as the only TeensyROM file a module has');

  const source = 'host_abi_standalone.cpp';
  const alone = vendorDir(MODULE_ABI, HOST_ABI, `vm/tests/${source}`);
  const exe = path.join(alone, 'standalone' + (process.platform === 'win32' ? '.exe' : ''));
  const compile = spawnSync(compiler, ['-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
    ...(process.platform === 'win32' ? ['-static'] : []), source, '-o', exe],
    { cwd: alone, encoding: 'utf8', windowsHide: true, maxBuffer: 16 * 1024 * 1024 });
  if (compile.error || compile.status || compile.signal) {
    process.stderr.write(`${compile.stdout ?? ''}${compile.stderr ?? ''}`);
    throw new Error(`${source} did not build from a directory holding only itself and the two published headers`);
  }
  process.stdout.write(run(exe, [], 'standalone host contract'));

  // vm/abi/vm_abi.h is included by the native tests, but nothing in the tree
  // includes vm_host_abi.h, so a broken relative path in it would go unnoticed.
  const shim = path.join(output, 'abi-shims.cpp');
  fs.writeFileSync(shim, '#include "vm/abi/vm_abi.h"\n#include "vm/abi/vm_host_abi.h"\nint main(){return 0;}\n');
  run(compiler, ['-std=c++17', '-fsyntax-only', '-I', root, shim], 'compiling the vm/abi shims');
  console.log('PASS: vm/abi/vm_abi.h and vm/abi/vm_host_abi.h resolve to the headers they publish');
}

checkBootSlot();
checkHostIdOffset();
checkEepromProtocol();
checkRam2Sizes();
checkServiceRegistry();
checkPublishedIncludes();
checkPublishedHeadersStandalone();

process.stdout.write(run(process.execPath, ['--test', 'tools/lib/extension.test.mjs'], 'package format unit tests'));

// One fixture tree, written by the packager, read by every C++ test below.
const fixture = registryFixture(sandbox('fixture-'));
fs.mkdirSync(path.join(fixture, 'VMS/HELLO/DATA'), { recursive: true });
fs.writeFileSync(path.join(fixture, 'VMS/HELLO/DATA/Sample.hi'), 'content');

native('files_test', [sandbox('files-sandbox-')]);
native('image_test', [path.join(fixture, 'VMS/HELLO/engine.mvm')]);
native('registry_test', [fixture, sandbox('registry-sandbox-')]);
native('launch_test', [fixture, sandbox('launch-sandbox-')]);
native('listing_test');
native('scheduler_test');
native('fail_test');
native('hello_module_test', [sandbox('hello-sandbox-')]);
native('host_install_test', [hostPackageFixture(sandbox('host-package-'))]);

if (keep) console.log(`Artifacts kept in ${output}`);
else fs.rmSync(output, { recursive: true, force: true });
console.log('PASS: extension loader conformance (published host contract, package format, file services, image validation, registry, launch routing, listing invalidation, failure reporting, host-package installation, reference module)');
