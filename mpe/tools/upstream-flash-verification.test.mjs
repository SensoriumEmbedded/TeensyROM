// SPDX-License-Identifier: MIT
// Source contracts for the current upstream updater, which retains legacy HEX parsing.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';

const read=name=>fs.readFileSync(new URL('../../Source/Teensy/'+name,import.meta.url),'utf8');

test('startup keeps normal menu entry without an automatic recovery-flash route',()=>{
  const startup=read('Teensy.ino');
  assert.doesNotMatch(startup,/RecoveryFlashAtPowerOn|RecoveryButtonsHeld|RecoveryFirmwarePath|RESTORE\.HEX|DoFlashUpdate/);
  assert.match(read('MinimalBoot/MinimalBoot.ino'),/MinBootInd_ExecuteMin \|\| ReadButton==0\) runMainTRApp\(\);/);
  assert.match(startup,/SetUpMainMenuROM\(\);/);
});

test('legacy updater retains target admission before its flash move',()=>{
  const flasher=read('Flash/FXUtil.cpp');
  const target=flasher.search(/if \(check_flash_id\( buffer_addr, hex\.max - hex\.min, FLASH_ID \)\)/);
  const move=flasher.search(/^\s*flash_move\( FLASH_BASE_ADDR, buffer_addr, hex\.max-hex\.min \);/m);
  assert.ok(target>=0&&move>target,'Target ID check must precede the actual flash move');
  assert.match(flasher.slice(target,move),/return;/,'Failed target admission retains an abort path');
});
