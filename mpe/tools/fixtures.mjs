// SPDX-License-Identifier: MIT
// Synthetic format fixtures for loader tests. No emulator or game payloads.
import fs from 'node:fs';
import path from 'node:path';
const crc=b=>{let c=0xffffffff;for(const v of b){c^=v;for(let i=0;i<8;i++)c=(c>>>1)^((c&1)?0xedb88320:0);}return(c^0xffffffff)>>>0;};
export function registryFixture(root){
  const pkg=path.join(root,'VMS/NESVM');fs.mkdirSync(pkg,{recursive:true});
  const code=Buffer.from([0x70,0x47]),header=Buffer.alloc(64);
  [0x314d564d,2,64,2,0,0,0x18001,0x18000,0x20014000,31,crc(code)].forEach((v,i)=>header.writeUInt32LE(v,i*4));
  header.writeUInt32LE(crc(header),44);
  fs.writeFileSync(path.join(pkg,'engine.mvm'),Buffer.concat([header,code]));
  fs.writeFileSync(path.join(pkg,'manifest.vmi'),'VM1\nNESVM\nnes\nengine.mvm\nclient.crt\nEND\n');
  const client=Buffer.alloc(0x6070),bank0=Buffer.alloc(8192,0x11),bank1=Buffer.alloc(8192,0x22);
  bank0.copy(client,80);bank1.copy(client,80+8208);
  const descriptor=Buffer.alloc(128);descriptor.write('VMH1');descriptor[4]=2;
  descriptor.writeUInt32LE(crc(Buffer.concat([bank0,bank1])),8);descriptor.write('NESVM',16);
  descriptor.writeUInt32LE(crc(descriptor.subarray(0,124)),124);descriptor.copy(client,0x4070);
  fs.writeFileSync(path.join(pkg,'client.crt'),client);fs.writeFileSync(path.join(root,'NESVM.crt'),client);
  return root;
}
