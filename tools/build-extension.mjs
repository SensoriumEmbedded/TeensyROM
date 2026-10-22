// SPDX-License-Identifier: MIT
//
// Builds an extension package from module sources: compile, link against the
// module linker script for the chosen code window, measure the sections, and
// write /VMS/<id> with its manifest, module image and client cartridge.
//
//   node tools/build-extension.mjs --id HELLO --extensions hi \
//        --source vm/hello/hello.cpp --client build/c64/vmhello.bin
//
// --services is the mask of service bits the module cannot run without,
// defaulting to the base profile. See the registry in vm/abi/README.md.
//
// --code-kib picks the code window: 96 (the default, which every host takes)
// or 128, which needs a host whose code ends at or below 0x10000.
//
// The image and cartridge formats live in tools/lib/extension.mjs; this script
// only turns an ELF into the inputs that library wants, then reads its own
// output back through the same validation the firmware applies.
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { scanArgs } from './lib/cli-args.mjs';
import {
  buildImage, parseImage, buildManifest, buildClientCrt,
  CODE_BASE, CODE_LIMIT, DATA_BASE, DATA_BYTES, MODULE_SCRIPTS,
  BASE_SERVICES, ASSIGNED_SERVICES, HOST_SERVICES,
} from './lib/extension.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
// package.json bakes --id, --extensions, --source and --client-source into build:hello, and
// `npm run <script> -- ...` appends the caller's arguments after those, so the previous
// first-wins indexOf() meant `npm run build:hello -- --id OTHER` built HELLO and exited 0.
const { option, all, flag, given } = scanArgs(process.argv.slice(2), {
  options: ['--id', '--extensions', '--client', '--client-source', '--out', '--services',
            '--code-kib', '--toolchain', '--arduino-data'],
  repeatable: ['--source'],
  flags: ['--keep', '--allow-unassigned-services'],
});

const id = option('--id');
const extensions = option('--extensions');
const sources = all('--source');
const clientBinary = option('--client');
const clientSource = option('--client-source');
const outRoot = path.resolve(option('--out', path.join(root, 'build/extensions')));
const keep = flag('--keep');
const allowUnassignedServices = flag('--allow-unassigned-services');
if (!id || !extensions || !sources.length) {
  throw new Error('Use --id <NAME> --extensions <list> --source <file.cpp> [--source ...]\n' +
                  '    [--client <file.bin> | --client-source <file.a>] [--out <dir>]\n' +
                  '    [--services <bits>] [--allow-unassigned-services] [--code-kib 96|128]');
}

// The code window, and with it the linker script. 96 KiB links at a base every
// host takes; 128 KiB needs one whose code_floor is at or below 0x10000, which
// the loader checks and the preflight reports. Default to the portable one --
// a module that does not need the space should run everywhere.
const windowKiB = (base) => (CODE_LIMIT - base) / 1024;
const codeKiB = given('--code-kib') ? Number(option('--code-kib')) : windowKiB(CODE_BASE);
const codeBase = [...MODULE_SCRIPTS.keys()].find((base) => windowKiB(base) === codeKiB);
if (codeBase === undefined) {
  throw new Error(`--code-kib wants ${[...MODULE_SCRIPTS.keys()].map(windowKiB).join(' or ')}, ` +
                  `not ${option('--code-kib') || 'a bare flag'}`);
}
const codeScript = MODULE_SCRIPTS.get(codeBase);

const SERVICE_MASK = /^(0[xX][0-9a-fA-F]+|[0-9]+)$/;
function parseServices(text) {
  const value = SERVICE_MASK.test(text ?? '') ? Number(text) : NaN;
  if (!Number.isInteger(value) || value > 0xffffffff) {
    throw new Error(`--services wants one 32-bit mask such as 0x1001f, not ${text || 'a bare flag'}`);
  }
  return value >>> 0;
}
const requiredServices = given('--services') ? parseServices(option('--services')) : BASE_SERVICES;
const beyondHost = (requiredServices & ~HOST_SERVICES) >>> 0;
if (beyondHost) {
  console.warn(`Note: this module requires services 0x${beyondHost.toString(16)}, which the TeensyROM ` +
               'loader does not provide, so it refuses the module rather than launching it.');
}
if (clientBinary && clientSource) throw new Error('Pass either --client or --client-source, not both');

// --- Toolchain -------------------------------------------------------------
// Explicit prefix, then PATH, then the Teensy core's own bundled ARM tools,
// which is what tools/build-firmware.mjs compiles the firmware with.
const TEENSY_ARM = 'packages/teensy/tools/teensy-compile/11.3.1/arm/bin/arm-none-eabi-';
function defaultArduinoDataDir() {
  if (process.env.ARDUINO_DIRECTORIES_DATA) return process.env.ARDUINO_DIRECTORIES_DATA;
  if (process.platform === 'win32') return path.join(process.env.LOCALAPPDATA ?? '', 'Arduino15');
  if (process.platform === 'darwin') return path.join(os.homedir(), 'Library/Arduino15');
  return path.join(os.homedir(), '.arduino15');
}
function resolvePrefix() {
  const explicit = option('--toolchain', process.env.ARM_TOOLCHAIN_PREFIX);
  if (explicit) return explicit;
  const probe = spawnSync(process.platform === 'win32' ? 'where' : 'which', ['arm-none-eabi-g++'], { encoding: 'utf8' });
  if (probe.status === 0) return 'arm-none-eabi-';
  const bundled = path.join(path.resolve(option('--arduino-data', defaultArduinoDataDir())), TEENSY_ARM);
  if (fs.existsSync(bundled + 'g++') || fs.existsSync(bundled + 'g++.exe')) return bundled;
  throw new Error(
    'No ARM toolchain found. Install the Teensy core (the same one tools/build-firmware.mjs uses),\n' +
    'put arm-none-eabi-g++ on PATH, or pass --toolchain /path/to/arm-none-eabi-');
}
const prefix = resolvePrefix();
const tool = (name) => prefix + name;

function run(exe, argv, label) {
  const result = spawnSync(exe, argv, { cwd: root, encoding: 'utf8', windowsHide: true, maxBuffer: 32 * 1024 * 1024 });
  if (result.error || result.status) {
    process.stderr.write(`${result.stdout ?? ''}${result.stderr ?? ''}`);
    throw new Error(`${label} failed (${result.error ?? 'exit ' + result.status})`);
  }
  return result.stdout ?? '';
}

// Matches the Teensy 4.1 core: Cortex-M7 with the double-precision FPU. A
// module compiled for a different float ABI links, then faults on target.
const CPU = ['-mcpu=cortex-m7', '-mthumb', '-mfloat-abi=hard', '-mfpu=fpv5-d16'];
const COMPILE = [...CPU, '-std=c++17', '-Os', '-ffreestanding', '-fno-exceptions', '-fno-rtti',
  '-fno-threadsafe-statics', '-fno-unwind-tables', '-fno-asynchronous-unwind-tables',
  '-ffunction-sections', '-fdata-sections', '-Wall', '-Wextra'];

const work = fs.mkdtempSync(path.join(os.tmpdir(), 'vm-extension-'));

// GCC emits calls to memset/memcpy from ordinary C++ (zeroing a struct, copying
// one) whether or not the source mentions them, and a freestanding module has no
// libc to resolve them against. Link the ABI's weak shims into every module; a
// module that defines its own overrides them without any build change.
const runtime = path.join(work, 'vm_runtime.o');
run(tool('gcc'), [...CPU, '-std=c11', '-Os', '-ffreestanding', '-fno-tree-loop-distribute-patterns',
  '-ffunction-sections', '-fdata-sections', '-Wall', '-Wextra',
  '-c', path.join(root, 'vm/abi/vm_runtime.c'), '-o', runtime], 'compiling the module runtime');

const objects = sources.map((source) => {
  const object = path.join(work, path.basename(source).replace(/\.\w+$/, '') + '.o');
  run(tool('g++'), [...COMPILE, '-c', path.resolve(root, source), '-o', object], `compiling ${source}`);
  return object;
});
const elf = path.join(work, id + '.elf');
run(tool('g++'), [...CPU, '-nostdlib', '-Wl,--gc-sections',
  '-T', path.join(root, codeScript), ...objects, runtime, '-o', elf], 'linking');

// --- Measure ---------------------------------------------------------------
const headers = run(tool('readelf'), ['-h', elf], 'reading ELF header');
const entryMatch = headers.match(/Entry point address:\s*(0x[0-9a-fA-F]+)/);
if (!entryMatch) throw new Error('Could not read the entry point from the linked module');
const entry = parseInt(entryMatch[1], 16);

// size -A prints one row per section: name, size, address.
const sections = new Map(run(tool('size'), ['-A', elf], 'measuring sections').split('\n')
  .map((line) => line.trim().split(/\s+/)).filter((row) => row.length >= 3)
  .map(([name, bytes, addr]) => [name, { bytes: Number(bytes), addr: Number(addr) }]));
const bssBytes = sections.get('.bss')?.bytes ?? 0;

// Where the linker actually put the code. buildImage's entry check only
// notices a --code-kib that disagrees with the script while the two windows
// do not overlap at the entry; past 32 KiB of .text they do, and a mismatch
// packages clean. The section address says it outright.
if (!sections.has('.text')) throw new Error('Could not read the address of .text from the linked module');
const linkedBase = sections.get('.text').addr;
if (linkedBase !== codeBase) {
  throw new Error(`--code-kib ${codeKiB} packages at 0x${codeBase.toString(16)}, but ${codeScript} linked .text at 0x${linkedBase.toString(16)}`);
}

const binary = (section) => {
  const file = path.join(work, section.slice(1) + '.bin');
  run(tool('objcopy'), ['-O', 'binary', '--only-section=' + section, elf, file], `extracting ${section}`);
  return fs.readFileSync(file);
};
const code = binary('.text');
const data = binary('.data');

// --- Package ---------------------------------------------------------------
const image = buildImage({ code, data, bssBytes, entry, requiredServices, allowUnassignedServices, codeBase });
// Read our own output back with the same checks the firmware applies, so a
// packaging mistake fails here rather than on the C64. A --code-kib that
// disagrees with the script that linked it is already out at the .text address
// above; this is the rest of the header agreeing with the module.
const header = parseImage(image);
if (header.entry !== entry || header.codeBase !== codeBase || header.ramBase !== DATA_BASE) {
  throw new Error('Packaged image disagrees with the linked module');
}

const directory = path.join(outRoot, 'VMS', id);
fs.mkdirSync(directory, { recursive: true });
fs.writeFileSync(path.join(directory, 'engine.mvm'), image);
fs.writeFileSync(path.join(directory, 'manifest.vmi'), buildManifest({ id, extensions }));

// The client cartridge is 6502, so it needs ACME rather than the ARM toolchain.
// Assembling it here keeps the whole package reproducible from sources.
function assembleClient(source) {
  const absolute = path.resolve(root, source);
  const binary = path.join(work, id + '-client.bin');
  const probe = spawnSync(process.platform === 'win32' ? 'where' : 'which', ['acme'], { encoding: 'utf8' });
  if (probe.status !== 0) throw new Error('--client-source needs the ACME assembler on PATH');
  run('acme', ['--outfile', binary, '-f', 'plain', absolute], `assembling ${source}`);
  return fs.readFileSync(binary);
}

if (clientBinary || clientSource) {
  const client = clientSource ? assembleClient(clientSource) : fs.readFileSync(path.resolve(root, clientBinary));
  if (client.length > 16384) throw new Error(`Client binary is ${client.length} bytes; the cartridge holds 16384`);
  const cartridge = buildClientCrt({ id, bank0: client.subarray(0, 8192), bank1: client.subarray(8192) });
  fs.writeFileSync(path.join(directory, 'client.crt'), cartridge);
  // A copy at the card root is how a user launches the package directly.
  fs.writeFileSync(path.join(outRoot, id + '.crt'), cartridge);
} else {
  console.log('No client given: the package has no cartridge and will not pass preflight.');
}

if (keep) console.log(`Build tree kept in ${work}`);
else fs.rmSync(work, { recursive: true, force: true });

const free = DATA_BYTES - (data.length + bssBytes);
console.log(`${id}: code ${code.length} of ${CODE_LIMIT - codeBase} bytes at $${codeBase.toString(16)}, ` +
            `data ${data.length}, bss ${bssBytes}, workspace left for the module ${free} bytes`);
console.log(`Package written to ${directory}`);
