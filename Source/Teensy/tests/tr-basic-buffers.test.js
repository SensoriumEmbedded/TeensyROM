import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';

const teensyIno = fs.readFileSync(path.resolve(import.meta.dirname, '..', 'Teensy.ino'), 'utf8');
const menuSetup = (() => {
  const start = teensyIno.indexOf('void SetUpMainMenuROM()');
  return teensyIno.slice(start, teensyIno.indexOf('\n}\n', start));
})();

test('returning to the menu frees the TPUT queue', () => {
  assert.match(menuSetup, /free\(\(void\*\)TputQueue\); TputQueue=NULL;/);
});

test("TR BASIC's buffers are freed only once its IO1 handler is swapped out", () => {
  const swap = menuSetup.indexOf('IOHandlerInit(IOH_TeensyROM)');
  assert.notEqual(swap, -1);
  for (const buffer of ['TgetQueue', 'LSFileName', 'TputQueue']) {
    const freed = menuSetup.search(new RegExp(`free\\((\\(void\\*\\))?${buffer}\\)`));
    assert.ok(freed > swap, `${buffer} is freed before the handler switch`);
  }
});
