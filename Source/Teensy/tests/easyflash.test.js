// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

const root = path.resolve(import.meta.dirname, '..');
const read = (file) => fs.readFileSync(path.join(root, file), 'utf8');
function between(source, first, last) {
  const start = source.indexOf(first), end = source.indexOf(last, start + first.length);
  assert(start >= 0 && end > start, `Missing production source anchor: ${first}`);
  return source.slice(start, end);
}

test('actual CRT loader and EasyFlash handler accept 1/2/4 MiB and reject malformed images', (t) => {
  const candidates = process.env.CXX ? [process.env.CXX] :
    ['g++', 'clang++', ...(process.platform === 'win32' ? ['C:/msys64/mingw32/bin/g++.exe'] : [])];
  const compiler = candidates.find((command) => spawnSync(command, ['--version'], { windowsHide: true }).status === 0);
  if (!compiler) return t.skip('Native C++ compiler unavailable; set CXX to g++ or clang++ to run the firmware regression harness');
  const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'teensyrom-easyflash-'));
  try {
    const loader = read('MinimalBoot/Min_DriveDirLoad.ino');
    const fullLoader = read('DriveDirLoad.ino'), fullParser = read('FileParsers.ino');
    const format = read('MinimalBoot/Common/DriveDirLoad.h');
    const handler = read('MinimalBoot/Common/IO_Handlers/IOH_EasyFlash.c');
    // Host pointers may be 64 bits; retain the firmware's tagged-pointer model
    // while moving its EF marker outside native addresses. No cartridge logic
    // is substituted. RAM2 ownership is tracked instead of using Teensy's fixed
    // 0x20200000 address range. These adapters also work with a 32-bit compiler.
    const native = (source) => source.replaceAll('(uint32_t)', '(uintptr_t)')
      .replace(/\(uintptr_t\)CrtChips\[cnt\]\.ChipROM\s*>=\s*0x20200000\s*&&\s*\(uintptr_t\)CrtChips\[cnt\]\.ChipROM\s*<\s*SwapSeekAddrMask/g,
        'isRAM2(CrtChips[cnt].ChipROM)')
      .replace(/\(uintptr_t\)CrtChips\[cnt\]\.ChipROM\s*>=\s*0x20200000/g, 'isRAM2(CrtChips[cnt].ChipROM)')
      .replace(/uint32_t Offset;/g, 'uintptr_t Offset;')
      .replace(/uint32_t (imageStartAddress|firstInstructionPtr)/g, 'uintptr_t $1');
    fs.writeFileSync(path.join(temporary, 'crt-format.h'),
      between(format, '#define CRT_MAIN_HDR_LEN', '#ifndef MinimumBuild')
        .replace(/^#define SwapSeekAddrMask.*$/m, '#define SwapSeekAddrMask (uintptr_t{0xef} << (sizeof(uintptr_t)*8-8))'));
    fs.writeFileSync(path.join(temporary, 'crt-table.h'),
      native(between(loader, 'uint16_t NumCrtChips', 'File myFile')));
    fs.writeFileSync(path.join(temporary, 'crt-loader.h'),
      native(between(loader, 'bool LoadFile(', 'bool PathIsRoot(')));
    fs.writeFileSync(path.join(temporary, 'crt-config.h'),
      between(loader, 'bool SetTypeFromCRT(', '//Big endian byte to int conversions:'));
    fs.writeFileSync(path.join(temporary, 'easyflash-handler.h'), native(handler));
    fs.writeFileSync(path.join(temporary, 'full-parser.h'), native(
      between(fullParser, 'uint8_t *ptrRAM_ImageEnd', 'char* StrSIDInfo') +
      between(fullParser, 'bool ParseCRTHeader(', 'FLASHMEM bool ParseARTHeader()')));
    fs.writeFileSync(path.join(temporary, 'full-loader.h'), native(
      between(fullLoader, 'bool LoadFile(', 'void InitDriveDirMenu()') +
      between(fullLoader, 'void FreeCrtChips()', 'uint8_t Assoc_Ext_ItemType(') +
      between(fullParser, 'bool SetTypeFromCRT(', '//Big endian byte to int conversions:')));
    // Execute the real menu transition through cartridge retirement. The
    // unrelated peripheral teardown after FreeCrtChips is outside this harness.
    fs.writeFileSync(path.join(temporary, 'full-retire.h'),
      between(read('Teensy.ino'), 'FLASHMEM void SetUpMainMenuROM()', '#ifdef Fab04_REU') + '}\n');
    fs.writeFileSync(path.join(temporary, 'remote-retire.h'), 'void RetireRemoteCartridge() {\n' +
      between(read('RemoteControl.ino'), '   if (CurrentIOHandler != IOH_TeensyROM)', '#ifdef Fab04_REU') + '}\n');
    // Reuse the repository's built-in image rather than adding game data.
    const bundled = between(read('TRMenuFiles/ROMs/Robocop_2_EasyFlash.crt.h'), '[] = {', '};');
    const bundledFile = path.join(temporary, 'bundled.crt');
    fs.writeFileSync(bundledFile, Buffer.from(bundled.match(/0x[\da-f]{2}/gi).map((byte) => Number(byte))));
    const environment = { ...process.env, PATH: path.dirname(compiler) + path.delimiter + process.env.PATH };
    for (const profile of ['native', 'vm-host']) {
      const executable = path.join(temporary, profile + (process.platform === 'win32' ? '.exe' : ''));
      const compile = spawnSync(compiler, ['-std=c++17', '-O2', '-UNDEBUG', '-I', temporary,
        ...(profile === 'vm-host' ? ['-DVM_HOST_PROFILE'] : []),
        path.join(import.meta.dirname, 'easyflash-harness.cpp'), '-o', executable],
      { encoding: 'utf8', windowsHide: true, env: environment });
      assert.equal(compile.status, 0, `${profile}: ${compile.error ?? ''}\n${compile.stdout}\n${compile.stderr}`);
      const result = spawnSync(executable, [bundledFile], { encoding: 'utf8', windowsHide: true, env: environment });
      assert.equal(result.status, 0, `${profile}: ${result.error ?? ''}\n${result.stdout}\n${result.stderr}`);
      assert.match(result.stdout, /PASS EasyFlash/);
      t.diagnostic(result.stdout.trim());
    }
  } finally {
    fs.rmSync(temporary, { recursive: true, force: true });
  }
});
