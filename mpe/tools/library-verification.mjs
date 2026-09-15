// SPDX-License-Identifier: MIT
import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import crypto from 'node:crypto';
import {MAIN_BASE,VM_BASE,VM_LIMIT} from './hex.mjs';
import {MPE_VERSION,MPE_LIBRARY_VERSION} from './build-identity.mjs';

const sha=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');

// Source compatibility tests cannot establish which implementation was linked.
// Check the package, build snapshot, and staged public adapter independently.
export function verifyLibraryInputs(build,root){
  const library=build.library;
  assert.ok(library,'Missing MPE library metadata');
  const folder=path.join(root,'mpe/library');
  assert.equal(path.resolve(library.manifestPath),path.join(folder,'manifest.json'));
  assert.equal(sha(library.manifestPath),library.manifestSha256,'MPE library manifest drift');
  const manifest=JSON.parse(fs.readFileSync(library.manifestPath,'utf8'));
  assert.equal(manifest.schemaVersion,1);
  assert.equal(manifest.version,MPE_LIBRARY_VERSION);
  assert.equal(manifest.entrypointAbi,1);
  assert.equal(manifest.flashBase,VM_BASE);
  assert.equal(manifest.flashLimit,VM_LIMIT);
  assert.equal(manifest.mainBase,MAIN_BASE);
  assert.equal(manifest.archive,'libMPEPrismHost.a');
  assert.deepEqual(manifest.publicHeaders,['include/MpeHost.h']);
  for(const key of ['version','packageRevision','sha256','entrypointAbi','flashBase','flashLimit','mainBase','sourceFirmwareSha256']){
    assert.equal(library[key],manifest[key],'MPE library report differs: '+key);
  }
  assert.equal(build.identity?.mpeVersion,MPE_VERSION,'Artifact integration version differs');
  assert.equal(path.resolve(library.archive),path.join(folder,manifest.archive));
  assert.equal(sha(library.archive),manifest.sha256,'MPE library archive drift');
  assert.equal(sha(path.join(build.runRoot,manifest.archive)),manifest.sha256,'Linked archive differs from package');
  const inputs=new Map(build.inputs.map(input=>[input.path,input.sha256]));
  const files=[];
  const walk=dir=>{for(const entry of fs.readdirSync(dir,{withFileTypes:true})){
    const file=path.join(dir,entry.name);
    if(entry.isDirectory())walk(file);
    else {
      const relative=path.relative(root,file).replaceAll('\\','/'),hash=sha(file);
      assert.equal(inputs.get(relative),hash,'Library file missing or changed in build inputs: '+relative);
      files.push({path:relative,sha256:hash});
    }
  }};
  walk(folder);
  const header=fs.readFileSync(path.join(folder,'include/MpeHost.h'),'utf8');
  assert.match(header,new RegExp('#define MPE_HOST_LIBRARY_VERSION "'+MPE_LIBRARY_VERSION.replaceAll('.','\\.')+'"'));
  assert.match(header,/#define MPE_HOST_LIBRARY_ABI 1\b/);
  assert.match(header,/void mpeHostSetup\(\);/);
  assert.match(header,/void mpeHostLoop\(\);/);
  const stub=fs.readFileSync(path.join(folder,'MPEBoot/MPEBoot.ino'),'utf8');
  assert.match(stub,/#include "\.\.\/include\/MpeHost\.h"/);
  assert.match(stub,/void setup\(\)\s*\{\s*mpeHostSetup\(\);\s*\}/);
  assert.match(stub,/void loop\(\)\s*\{\s*mpeHostLoop\(\);\s*\}/);
  const staged=path.join(build.runRoot,'source/Source/Teensy/MPEBoot');
  assert.equal(fs.readFileSync(path.join(staged,'MpeHost.h'),'utf8'),header,'Staged public header differs from package');
  assert.equal(fs.readFileSync(path.join(staged,'MPEBoot.ino'),'utf8'),stub.replace('../include/MpeHost.h','MpeHost.h'),'Staged public wrapper differs from package');
  assert.equal(fs.existsSync(path.join(staged,'VMHostPoll.h')),false,'Library build unexpectedly stages the legacy scheduler');
  return {version:manifest.version,packageRevision:manifest.packageRevision,
    archiveSha256:manifest.sha256,manifestSha256:library.manifestSha256,
    entrypointAbi:manifest.entrypointAbi,verifiedPackageFiles:files.length,
    scope:'Package and staged adapter byte integrity; linked entry points and memory checks are verified separately.'};
}

export function verifyLibrarySymbols(symbols){
  const names=['mpeHostSetup()','mpeHostLoop()','VMHostPoll()',
    'VmRuntime::submitPrismPlus(VmIndexedFrame*)'];
  for(const name of names){
    const escaped=name.replace(/[.*+?^${}()|[\]\\]/g,'\\$&');
    const match=symbols.match(new RegExp('^([0-9a-f]+) [Tt] '+escaped+'$','m'));
    assert.ok(match,'Missing linked MPE/Prism entry: '+name);
    const address=parseInt(match[1],16);
    assert.ok(address<0x18000||(address>=VM_BASE&&address<VM_LIMIT),'MPE/Prism entry outside host code: '+name);
  }
  return names;
}
