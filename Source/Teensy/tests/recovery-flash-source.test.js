'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const read = name => fs.readFileSync(path.resolve(__dirname, '..', name), 'utf8');

test('startup cannot flash from a two-button hold or a specially named SD file', () => {
  const startup = read('Teensy.ino');
  assert.doesNotMatch(startup, /RecoveryFlashAtPowerOn|RecoveryButtonsHeld|RecoveryFirmwarePath|RESTORE\.HEX|DoFlashUpdate/);
  // Preserve the existing Menu-only exit from a large-cartridge launch.
  assert.match(read('MinimalBoot/MinimalBoot.ino'), /MinBootInd_ExecuteMin \|\| ReadButton==0\) runMainTRApp\(\);/);
  assert.match(startup, /SetUpMainMenuROM\(\);/);
});

test('normal firmware update still validates the target before moving flash', () => {
  const flasher = read('Flash/FXUtil.cpp');
  assert.match(flasher, /check_flash_id\( buffer_addr, image_size, FLASH_ID \)/);
  assert.match(flasher, /flash_move\( FLASH_BASE_ADDR, buffer_addr, image_size \);/);
});
