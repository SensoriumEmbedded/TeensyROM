// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {execFileSync} from 'node:child_process';

const root=path.resolve(import.meta.dirname,'../..');
const upstream='dc1174ce8475153160e0b0da4ff65525a7dd4e5a';
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
