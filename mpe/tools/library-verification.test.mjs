// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import crypto from 'node:crypto';
import {verifyLibraryInputs,verifyLibrarySymbols} from './library-verification.mjs';
import {MPE_VERSION,MPE_LIBRARY_VERSION} from './build-identity.mjs';
import {MAIN_BASE,VM_BASE,VM_LIMIT} from './hex.mjs';

const sha=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
function fixture(t){
  const root=fs.mkdtempSync(path.join(os.tmpdir(),'mpe-library-check-'));
  t.after(()=>{assert.equal(path.dirname(root),path.resolve(os.tmpdir()));fs.rmSync(root,{recursive:true,force:true});});
  const folder=path.join(root,'mpe/library'),runRoot=path.join(root,'build/run');
  const staged=path.join(runRoot,'source/Source/Teensy/MPEBoot');
  for(const dir of [path.join(folder,'include'),path.join(folder,'MPEBoot'),staged])fs.mkdirSync(dir,{recursive:true});
  const write=(file,text)=>fs.writeFileSync(path.join(folder,file),text);
  const archive=path.join(folder,'libMPEPrismHost.a');
  write('libMPEPrismHost.a','Synthetic archive bytes: hashing fixture only.');
  const manifest={schemaVersion:1,version:MPE_LIBRARY_VERSION,packageRevision:'test-1',entrypointAbi:1,
    flashBase:VM_BASE,flashLimit:VM_LIMIT,mainBase:MAIN_BASE,
    archive:'libMPEPrismHost.a',sha256:sha(archive),sourceFirmwareSha256:'0'.repeat(64),publicHeaders:['include/MpeHost.h']};
  write('manifest.json',JSON.stringify(manifest));
  write('include/MpeHost.h',`#define MPE_HOST_LIBRARY_VERSION "${MPE_LIBRARY_VERSION}"\n#define MPE_HOST_LIBRARY_ABI 1\nvoid mpeHostSetup();\nvoid mpeHostLoop();\n`);
  write('MPEBoot/MPEBoot.ino','#include "../include/MpeHost.h"\nvoid setup() { mpeHostSetup(); }\nvoid loop() { mpeHostLoop(); }\n');
  write('LICENSE.txt','Synthetic license-file integrity fixture.');
  const files=['manifest.json','libMPEPrismHost.a','include/MpeHost.h','MPEBoot/MPEBoot.ino','LICENSE.txt'];
  const inputs=files.map(file=>({path:'mpe/library/'+file,sha256:sha(path.join(folder,file))}));
  fs.copyFileSync(archive,path.join(runRoot,manifest.archive));
  fs.copyFileSync(path.join(folder,'include/MpeHost.h'),path.join(staged,'MpeHost.h'));
  fs.writeFileSync(path.join(staged,'MPEBoot.ino'),fs.readFileSync(path.join(folder,'MPEBoot/MPEBoot.ino'),'utf8').replace('../include/MpeHost.h','MpeHost.h'));
  const manifestPath=path.join(folder,'manifest.json');
  const build={runRoot,identity:{mpeVersion:MPE_VERSION},inputs,library:{...manifest,archive,manifestPath,manifestSha256:sha(manifestPath)}};
  return {root,folder,staged,build};
}

test('a recorded package and exactly staged public adapter pass integrity checks',t=>{
  const f=fixture(t),result=verifyLibraryInputs(f.build,f.root);
  assert.equal(result.verifiedPackageFiles,5);
  assert.equal(result.archiveSha256,f.build.library.sha256);
});

test('a replaced archive fails even if other report metadata is unchanged',t=>{
  const f=fixture(t);fs.appendFileSync(f.build.library.archive,'changed');
  assert.throws(()=>verifyLibraryInputs(f.build,f.root),/archive drift/);
});

test('a different archive staged for the linker fails independently of the package',t=>{
  const f=fixture(t);fs.appendFileSync(path.join(f.build.runRoot,'libMPEPrismHost.a'),'changed');
  assert.throws(()=>verifyLibraryInputs(f.build,f.root),/Linked archive differs/);
});

test('manifest drift and report version disagreement fail',t=>{
  const f=fixture(t);f.build.library.version='0.0.0';
  assert.throws(()=>verifyLibraryInputs(f.build,f.root),/report differs: version/);
  f.build.library.version=MPE_LIBRARY_VERSION;fs.appendFileSync(f.build.library.manifestPath,'\n');
  assert.throws(()=>verifyLibraryInputs(f.build,f.root),/manifest drift/);
});

test('unrecorded package files and changed notices fail the build snapshot check',t=>{
  const f=fixture(t);fs.writeFileSync(path.join(f.folder,'unrecorded.txt'),'new');
  assert.throws(()=>verifyLibraryInputs(f.build,f.root),/missing or changed in build inputs/);
  fs.unlinkSync(path.join(f.folder,'unrecorded.txt'));fs.appendFileSync(path.join(f.folder,'LICENSE.txt'),'changed');
  assert.throws(()=>verifyLibraryInputs(f.build,f.root),/missing or changed in build inputs/);
});

test('staged public API or wrapper changes fail exact package comparison',t=>{
  const f=fixture(t),header=path.join(f.staged,'MpeHost.h');fs.appendFileSync(header,'\n// changed');
  assert.throws(()=>verifyLibraryInputs(f.build,f.root),/Staged public header differs/);
  fs.copyFileSync(path.join(f.folder,'include/MpeHost.h'),header);
  fs.appendFileSync(path.join(f.staged,'MPEBoot.ino'),'\nvoid unrelated() {}');
  assert.throws(()=>verifyLibraryInputs(f.build,f.root),/Staged public wrapper differs/);
});

test('a legacy scheduler cannot silently remain the staged library implementation',t=>{
  const f=fixture(t);fs.writeFileSync(path.join(f.staged,'VMHostPoll.h'),'legacy implementation');
  assert.throws(()=>verifyLibraryInputs(f.build,f.root),/unexpectedly stages the legacy scheduler/);
});

const symbols='00006a10 T mpeHostLoop()\n00006d54 T mpeHostSetup()\n00004f0c T VMHostPoll()\n6028a86c t VmRuntime::submitPrismPlus(VmIndexedFrame*)\n';
test('linked public and Prism entry points must all be defined in host code',()=>{
  assert.equal(verifyLibrarySymbols(symbols).length,4);
  assert.throws(()=>verifyLibrarySymbols(symbols.replace(' T mpeHostLoop()',' U mpeHostLoop()')),/Missing linked/);
  assert.throws(()=>verifyLibrarySymbols(symbols.replace('VmRuntime::submitPrismPlus','VmRuntime::submitLegacy')),/Missing linked/);
  assert.throws(()=>verifyLibrarySymbols(symbols.replace('6028a86c','20008000')),/outside host code/);
});
