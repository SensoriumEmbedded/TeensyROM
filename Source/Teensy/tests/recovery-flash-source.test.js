import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
const read = name => fs.readFileSync(path.resolve(import.meta.dirname, '..', name), 'utf8');

test('startup cannot flash from a two-button hold or a specially named SD file', () => {
  const startup = read('Teensy.ino');
  assert.doesNotMatch(startup, /RecoveryFlashAtPowerOn|RecoveryButtonsHeld|RecoveryFirmwarePath|RESTORE\.HEX|DoFlashUpdate/);
  // Preserve the existing Menu-only exit from a large-cartridge launch.
  assert.match(read('MinimalBoot/MinimalBoot.ino'), /MinBootInd_ExecuteMin \|\| ReadButton==0\) runMainTRApp\(\);/);
  assert.match(startup, /SetUpMainMenuROM\(\);/);
});

test('normal firmware update still validates the target before moving flash', () => {
  const flasher = read('Flash/FXUtil.cpp');
  // check_flash_id/flash_move take hex.max - hex.min (the parsed image span) since the
  // Intel HEX hardening revert (commit 80ba637) replaced the image_size local these
  // regexes originally matched -- image_size measured from FLASH_BASE_ADDR, a different
  // span -- with this inline expression.
  assert.match(flasher, /check_flash_id\(\s*buffer_addr,\s*hex\.max\s*-\s*hex\.min,\s*FLASH_ID\s*\)/);
  assert.match(flasher, /flash_move\(\s*FLASH_BASE_ADDR,\s*buffer_addr,\s*hex\.max\s*-\s*hex\.min\s*\);/);
});
