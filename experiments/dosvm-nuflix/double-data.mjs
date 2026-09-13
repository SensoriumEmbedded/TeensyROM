import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import {mirrorDisplay} from './double-buffer.mjs';

export function generateDoubleData(output){
  const template=fs.readFileSync(new URL('./upstream-pinned/NuflixStudio/Settings/nufli-template.bin',import.meta.url));
  const mapping=[...fs.readFileSync(path.join(output,'native-data.h'),'utf8').split('displayMapping[]')[1].matchAll(/0x([0-9a-f]+)/g)].map(match=>Number.parseInt(match[1],16));
  for(const address of [0x4000,0x43c0])for(let offset=0;offset<24;offset++){const entry=mapping[address-0x1000+offset];assert.ok((entry&0x8000)&&template[entry&0x7fff]===0,'Top dummy sprite bytes must never contain picture data');}
  const reference=Buffer.alloc(0x6a02);reference.writeUInt16LE(0x1000);
  template.copy(reference,0x1002,0,0x5a00);
  reference.writeUInt16LE(0x1001,template.readUInt16LE(2)-0x1000+2);
  reference[2]=0xa9;reference[3]=3;
  const edits=mirrorDisplay(reference,template).edits.filter(edit=>edit.address>=0x3000).map(edit=>[edit.address,(edit.after-edit.before)&255]);
  assert.ok(edits.length<128);
  const deltas=Array(0x1200).fill(0);
  for(const [address,delta] of edits)if(address<0x4200){assert.equal(deltas[address-0x3000],0);deltas[address-0x3000]=delta;}
  fs.writeFileSync(path.join(output,'double-data.h'),'#pragma once\nnamespace dosvm_nuflix {\nstatic const uint8_t mirrorDeltas[] NUFLIX_ROM = {\n'+deltas.join(',')+'\n};\n}\n');
}
