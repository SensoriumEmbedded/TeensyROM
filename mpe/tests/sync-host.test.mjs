// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {spawnSync} from 'node:child_process';

test('retired source importer fails closed without a library package or checkout',()=>{
  // An isolated copy has no package, lock or checkout. Rejection must occur
  // before either revision resolution or importing, even in this state.
  const dir=fs.mkdtempSync(path.join(os.tmpdir(),'mpe-source-import-'));
  const script=path.join(dir,'sync-host.mjs');
  fs.copyFileSync(new URL('../tools/sync-host.mjs',import.meta.url),script);
  const before=fs.readFileSync(script);
  for(const args of [[],['missing-private-checkout','HEAD']]){
    const result=spawnSync(process.execPath,[script,...args],{cwd:dir,encoding:'utf8',windowsHide:true});
    assert.notEqual(result.status,0);
    assert.match(result.stderr,/Source import is disabled for the compiled MPE host integration/);
    assert.deepEqual(fs.readdirSync(dir),['sync-host.mjs']);
    assert.deepEqual(fs.readFileSync(script),before);
  }
});
