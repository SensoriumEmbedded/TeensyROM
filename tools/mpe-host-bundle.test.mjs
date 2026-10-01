import assert from 'node:assert/strict';
import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import test from 'node:test';
import {parseHostPackage,hostDescriptor,parseImage,buildManifest,CLIENT_BYTES} from './lib/extension.mjs';

const directory=fileURLToPath(new URL('../bin/MPE/',import.meta.url));
const read=name=>fs.readFileSync(path.join(directory,name));
const sha=bytes=>crypto.createHash('sha256').update(bytes).digest('hex');

test('shipped MPE installer is accepted by the production stock package parser',()=>{
  const bytes=read('MPE.TRH'),header=parseHostPackage(bytes);
  const descriptor=hostDescriptor(bytes.subarray(header.headerBytes));
  assert.equal(header.name,'MHS MPE');assert.equal(header.abi,2);assert.equal(header.services,32);
  assert.equal(descriptor.hostBytes,76);assert.equal(descriptor.codeFloor,32244);
  assert(descriptor.codeFloor<=0x10000);
  const evidence=JSON.parse(read('VERIFICATION.json'));
  assert.equal(sha(bytes),evidence.hostSha256);
  assert.equal(header.imageBytes,evidence.imageBytes);
});

test('shipped registration requires the installed MPE discriminator and pairs its CRT',()=>{
  const host=parseHostPackage(read('MPE.TRH'));
  const module=parseImage(read('VMS/MPE/engine.mvm'));
  assert.equal(module.requiredServices,host.services);
  assert.equal(read('VMS/MPE/manifest.vmi').toString(),String(buildManifest({id:'MPE',extensions:['mpe']})));
  const client=read('VMS/MPE/client.crt');assert.equal(client.length,CLIENT_BYTES);
  assert.equal(client.subarray(0,16).toString(),'C64 CARTRIDGE   ');
  assert.equal(client.readUInt16BE(22),32);
});

test('shipped bundle hashes bind every install file to its hardware evidence',()=>{
  const manifest=JSON.parse(read('MANIFEST.json'));
  const hardware=JSON.parse(read('HARDWARE-TEST.json')).fullHostTrial;
  assert.equal(manifest.hostSha256,hardware.deliveredHostSha256);
  assert(hardware.ownerReportedWorking);
  const entries=new Set();
  for(const entry of manifest.files){
    assert(!entries.has(entry.path));entries.add(entry.path);
    assert(!path.isAbsolute(entry.path)&&!entry.path.split('/').includes('..'));
    const bytes=read(entry.path);assert.equal(bytes.length,entry.bytes);assert.equal(sha(bytes),entry.sha256);
  }
  for(const required of ['MPE.TRH','MPE-DEMO.MPE','VMS/MPE/manifest.vmi','VMS/MPE/engine.mvm','VMS/MPE/client.crt','HARDWARE-TEST.json','VERIFICATION.json'])assert(entries.has(required));
});
