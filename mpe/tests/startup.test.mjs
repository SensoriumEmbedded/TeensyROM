// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {execFileSync} from 'node:child_process';

const root=path.resolve(import.meta.dirname,'../..');
const upstream='80ba6378b4417b284d3e212f65befd8c9b25d968';
const read=file=>fs.readFileSync(path.join(root,file),'utf8').replaceAll('\r\n','\n');

test('ordinary startup and button handling match current upstream exactly',()=>{
  const file='Source/Teensy/Teensy.ino';
  const expected=execFileSync('git',['show',upstream+':'+file],{cwd:root,encoding:'utf8',windowsHide:true}).replaceAll('\r\n','\n');
  assert.equal(read(file),expected);
  assert.equal(fs.existsSync(path.join(root,'Source/Teensy/RecoveryFlash.h')),false);
});

test('normal browser update route remains available',()=>{
  assert.match(read('Source/Teensy/FlashUpdate.ino'),/void DoFlashUpdate\(FS \*sourceFS, const char \*FilePathName\)/);
  assert.match(read('Source/Teensy/DriveDirLoad.ino'),/DoFlashUpdate\(/);
});
