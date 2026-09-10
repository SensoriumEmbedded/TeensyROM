import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';

export function generateNativeData(upstream, output) {
  const template = fs.readFileSync(path.join(upstream,'NuflixStudio/Settings/nufli-template.bin'));
  for(const operand of [0x3118,0x31f7])assert.deepEqual(template.subarray(operand-0x2001,operand-0x2001+3),Buffer.from([0x20,0,0x10]),'Pinned speed-code call moved');
  const source = fs.readFileSync(path.join(upstream,'NuflixStudio/Assets/Scripts/NuflixFormat.cs'),'utf8');
  const table = name => [...source.match(new RegExp(name+' =\\s*\\{([^}]+)'))[1].matchAll(/0x[0-9a-f]+/g)].map(value => Number(value[0]));
  const screens=table('ScreenRamRowOffsets'),underlay=table('UnderlayRowOffsets'),hires=table('BugHiresSpriteRowOffsets'),multi=table('BugMultiSpriteRowOffsets');
  const mapping = Array.from({length:0x6a00},(_,index) => index<0x1100?18400+index:0x8000+index-0x1000);
  const place=(address,offset) => {assert.ok(address>=0x2100 && address<0x7a00);assert.ok(offset<22752);mapping[address-0x1000]=offset;};
  for(let offset=0;offset<8000;offset++)place(offset<0x1400?0x6000+offset:0x2000+offset,5600+offset);
  for(let column=0;column<40;column++)place(0x32c7+column*8,5600+0x12c7+column*8);
  for(let row=0;row<100;row++)for(let column=0;column<40;column++)place(0x2000+screens[row]+column,row*40+column);
  let spriteRow=5;
  for(let row=0;row<200;row++){
    for(let column=0;column<18;column++){
      const line=(spriteRow+column%3)&63;
      const offset=column>=15 && row<128?0x5400+(((row>>1)&7)^7)*64+line:underlay[row>>1]+line+Math.floor(column/3)*64;
      place(0x2000+offset,13600+row*24+column);
    }
    for(let column=0;column<3;column++){
      const line=(spriteRow+column)&63;
      place(0x2000+hires[row>>1]+line,13600+row*24+18+column);
      place(0x2000+multi[row>>1]+line,13600+row*24+21+column);
    }
    if(!(row&1))spriteRow+=3;
    if(spriteRow>63)spriteRow&=63;else if(spriteRow===63)spriteRow=0;
  }
  const array=(type,name,values) => `static const ${type} ${name}[] NUFLIX_ROM = {\n${Array.from({length:Math.ceil(values.length/24)},(_,row)=>values.slice(row*24,row*24+24).map(value=>'0x'+value.toString(16)).join(',')).join(',\n')}\n};\n`;
  const patches=Array(0x400).fill(0);
  const patch=(address,kind)=>{if(address<0x3000||address>=0x3400)return;const old=patches[address-0x3000];assert.ok(!old||old===kind);patches[address-0x3000]=kind;};
  for(const address of [0x3118,0x31f7]){patch(address,1);patch(address+1,2);}
  for(let standard=0;standard<2;standard++){
    for(let slot=2;slot<12;slot++)patch(template.readUInt16LE((slot+standard*12)*2),slot===2?0x11:slot===3?0x13:slot===4?0x10:slot===11?0x12:0x20+slot-5);
    patch(template.readUInt16LE(24+standard*24),0x30);patch(template.readUInt16LE(26+standard*24),0x31);
  }
  patch(template.readUInt16LE(2),0x32);patch(template.readUInt16LE(2)+1,0x33);
  patch(template.readUInt16LE(52),0x34);patch(template.readUInt16LE(54),0x35);patch(template.readUInt16LE(56),0x35);
  fs.writeFileSync(path.join(output,'native-data.h'),'#pragma once\n#include <stdint.h>\n#if defined(__arm__)\n#define NUFLIX_ROM __attribute__((section(".progmem")))\n#else\n#define NUFLIX_ROM\n#endif\nnamespace dosvm_nuflix {\n'+array('uint8_t','displayTemplate',[...template])+array('uint16_t','displayMapping',mapping)+array('uint8_t','displayPatches',patches)+'}\n');
}
